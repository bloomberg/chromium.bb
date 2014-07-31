// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_infobar_delegate.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

ExtensionInfoBarDelegate::~ExtensionInfoBarDelegate() {
}

// static
void ExtensionInfoBarDelegate::Create(content::WebContents* web_contents,
                                      Browser* browser,
                                      const extensions::Extension* extension,
                                      const GURL& url,
                                      int height) {
  InfoBarService::FromWebContents(web_contents)->AddInfoBar(
      ExtensionInfoBarDelegate::CreateInfoBar(
          scoped_ptr<ExtensionInfoBarDelegate>(new ExtensionInfoBarDelegate(
              browser, extension, url, web_contents, height))));
}

ExtensionInfoBarDelegate::ExtensionInfoBarDelegate(
    Browser* browser,
    const extensions::Extension* extension,
    const GURL& url,
    content::WebContents* web_contents,
    int height)
    : infobars::InfoBarDelegate(),
#if defined(TOOLKIT_VIEWS)
      browser_(browser),
#endif
      extension_(extension),
      extension_registry_observer_(this),
      closing_(false) {
  extension_view_host_.reset(
      extensions::ExtensionViewHostFactory::CreateInfobarHost(url, browser));
  extension_view_host_->SetAssociatedWebContents(web_contents);

  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(browser->profile()));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(browser->profile()));

  height_ = std::max(0, height);
  height_ = std::min(2 * infobars::InfoBar::kDefaultBarTargetHeight, height_);
  if (height_ == 0)
    height_ = infobars::InfoBar::kDefaultBarTargetHeight;
}

content::WebContents* ExtensionInfoBarDelegate::GetWebContents() {
  return InfoBarService::WebContentsFromInfoBar(infobar());
}

// ExtensionInfoBarDelegate::CreateInfoBar() is implemented in platform-specific
// files.

bool ExtensionInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  ExtensionInfoBarDelegate* extension_delegate =
      delegate->AsExtensionInfoBarDelegate();
  // When an extension crashes, an InfoBar is shown (for the crashed extension).
  // That will result in a call to this function (to see if this InfoBarDelegate
  // is already showing the 'extension crashed InfoBar', which it never is), but
  // if it is our extension that crashes, the extension delegate is NULL so
  // we cannot check.
  if (!extension_delegate)
    return false;

  // Only allow one InfoBar at a time per extension.
  return extension_delegate->extension_view_host()->extension() ==
         extension_view_host_->extension();
}

void ExtensionInfoBarDelegate::InfoBarDismissed() {
  closing_ = true;
}

infobars::InfoBarDelegate::Type ExtensionInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}

ExtensionInfoBarDelegate*
    ExtensionInfoBarDelegate::AsExtensionInfoBarDelegate() {
  return this;
}

void ExtensionInfoBarDelegate::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (extension_ == extension)
    infobar()->RemoveSelf();
}

void ExtensionInfoBarDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE);
  if (extension_view_host_.get() ==
      content::Details<extensions::ExtensionHost>(details).ptr())
    infobar()->RemoveSelf();
}
