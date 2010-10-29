// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_disabled_infobar_delegate.h"

#include <string>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/browser_list.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

class ExtensionDisabledDialogDelegate
    : public ExtensionInstallUI::Delegate,
      public base::RefCountedThreadSafe<ExtensionDisabledDialogDelegate> {
 public:
  ExtensionDisabledDialogDelegate(Profile* profile,
                                  ExtensionsService* service,
                                  const Extension* extension)
        : service_(service), extension_(extension) {
    AddRef();  // Balanced in Proceed or Abort.

    install_ui_.reset(new ExtensionInstallUI(profile));
    install_ui_->ConfirmInstall(this, extension_);
  }

  // Overridden from ExtensionInstallUI::Delegate:
  virtual void InstallUIProceed() {
    ExtensionPrefs* prefs = service_->extension_prefs();
    prefs->SetDidExtensionEscalatePermissions(extension_, false);
    service_->EnableExtension(extension_->id());
    Release();
  }
  virtual void InstallUIAbort() {
    // Do nothing. The extension will remain disabled.
    Release();
  }

 private:
  friend class base::RefCountedThreadSafe<ExtensionDisabledDialogDelegate>;

  virtual ~ExtensionDisabledDialogDelegate() {}

  // The UI for showing the install dialog when enabling.
  scoped_ptr<ExtensionInstallUI> install_ui_;

  ExtensionsService* service_;
  const Extension* extension_;
};

class ExtensionDisabledInfobarDelegate
    : public ConfirmInfoBarDelegate,
      public NotificationObserver {
 public:
  ExtensionDisabledInfobarDelegate(TabContents* tab_contents,
                                   ExtensionsService* service,
                                   const Extension* extension)
      : ConfirmInfoBarDelegate(tab_contents),
        tab_contents_(tab_contents),
        service_(service),
        extension_(extension) {
    // The user might re-enable the extension in other ways, so watch for that.
    registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                   Source<Profile>(service->profile()));
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
                   Source<Profile>(service->profile()));
  }
  virtual ~ExtensionDisabledInfobarDelegate() {
  }
  virtual string16 GetMessageText() const {
    return l10n_util::GetStringFUTF16(IDS_EXTENSION_DISABLED_INFOBAR_LABEL,
                                      UTF8ToUTF16(extension_->name()));
  }
  virtual SkBitmap* GetIcon() const {
    return NULL;
  }
  virtual int GetButtons() const {
    return BUTTON_OK;
  }
  virtual string16 GetButtonLabel(
      ConfirmInfoBarDelegate::InfoBarButton button) const {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSION_DISABLED_INFOBAR_ENABLE_BUTTON);
  }
  virtual bool Accept() {
    // This object manages its own lifetime.
    new ExtensionDisabledDialogDelegate(tab_contents_->profile(),
                                        service_, extension_);
    return true;
  }
  virtual void InfoBarClosed() {
    delete this;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    // TODO(mpcomplete): RemoveInfoBar doesn't seem to always result in us
    // getting deleted.
    switch (type.value) {
      case NotificationType::EXTENSION_LOADED:
      case NotificationType::EXTENSION_UNLOADED_DISABLED: {
        const Extension* extension = Details<const Extension>(details).ptr();
        if (extension == extension_)
          tab_contents_->RemoveInfoBar(this);
        break;
      }
      default:
        NOTREACHED();
    }
  }

 private:
  NotificationRegistrar registrar_;
  TabContents* tab_contents_;
  ExtensionsService* service_;
  const Extension* extension_;
};

void ShowExtensionDisabledUI(ExtensionsService* service, Profile* profile,
                             const Extension* extension) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (!browser)
    return;

  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return;

  tab_contents->AddInfoBar(new ExtensionDisabledInfobarDelegate(
      tab_contents, service, extension));
}

void ShowExtensionDisabledDialog(ExtensionsService* service, Profile* profile,
                                 const Extension* extension) {
  // This object manages its own lifetime.
  new ExtensionDisabledDialogDelegate(profile, service, extension);
}
