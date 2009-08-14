// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_disabled_infobar_delegate.h"

#include "app/l10n_util.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/browser_list.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

class ExtensionDisabledInfobarDelegate
    : public ConfirmInfoBarDelegate,
      public NotificationObserver {
 public:
  ExtensionDisabledInfobarDelegate(TabContents* tab_contents,
                                   ExtensionsService* service,
                                   const std::string& extension_id,
                                   const std::string& extension_name)
      : ConfirmInfoBarDelegate(tab_contents),
        tab_contents_(tab_contents),
        service_(service),
        extension_id_(extension_id),
        extension_name_(extension_name) {
    // The user might re-enable the extension in other ways, so watch for that.
    registrar_.Add(this, NotificationType::EXTENSIONS_LOADED,
                   Source<ExtensionsService>(service));
  }
  virtual void InfoBarClosed() {
    delete this;
  }
  virtual std::wstring GetMessageText() const {
    return l10n_util::GetStringF(IDS_EXTENSION_DISABLED_INFOBAR_LABEL,
                                 UTF8ToWide(extension_name_));
  }
  virtual SkBitmap* GetIcon() const {
    return NULL;
  }
  virtual int GetButtons() const {
    return BUTTON_OK;
  }
  virtual std::wstring GetButtonLabel(
      ConfirmInfoBarDelegate::InfoBarButton button) const {
    return l10n_util::GetString(IDS_EXTENSION_DISABLED_INFOBAR_ENABLE_BUTTON);
  }
  virtual bool Accept() {
    service_->EnableExtension(extension_id_);
    return true;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::EXTENSIONS_LOADED);
    ExtensionList* extensions = Details<ExtensionList>(details).ptr();

    for (ExtensionList::iterator iter = extensions->begin();
         iter != extensions->end(); ++iter) {
       if ((*iter)->id() == extension_id_) {
         // TODO(mpcomplete): This doesn't seem to always result in us getting
         // deleted.
         tab_contents_->RemoveInfoBar(this);
         return;
       }
    }
  }

 private:
  NotificationRegistrar registrar_;
  TabContents* tab_contents_;
  ExtensionsService* service_;
  std::string extension_id_;
  std::string extension_name_;
};

void ShowExtensionDisabledUI(ExtensionsService* service, Profile* profile,
                             Extension* extension) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (!browser)
    return;

  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return;

  tab_contents->AddInfoBar(new ExtensionDisabledInfobarDelegate(
      tab_contents, service, extension->id(), extension->name()));
}
