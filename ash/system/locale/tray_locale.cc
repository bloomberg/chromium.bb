// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/tray_locale.h"

#include "ash/system/tray/tray_views.h"
#include "base/string16.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"

namespace ash {
namespace internal {

namespace tray {

class LocaleNotificationView : public TrayNotificationView,
                               public views::LinkListener {
 public:
  LocaleNotificationView(TrayLocale* tray,
                         LocaleObserver::Delegate* delegate,
                         const std::string& cur_locale,
                         const std::string& from_locale,
                         const std::string& to_locale)
      : tray_(tray),
        delegate_(delegate) {
    string16 from = l10n_util::GetDisplayNameForLocale(
        from_locale, cur_locale, true);
    string16 to = l10n_util::GetDisplayNameForLocale(
        to_locale, cur_locale, true);

    views::View* container = new views::View;

    views::Label* message = new views::Label(
        l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_LOCALE_CHANGE_MESSAGE, from, to));
    container->AddChildView(message);

    views::Link* revert = new views::Link(
        l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_LOCALE_REVERT_MESSAGE, from));
    container->AddChildView(revert);

    InitView(container);
  }

  // Overridden from views::LinkListener.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE {
    if (delegate_)
      delegate_->RevertLocaleChange();
  }

  // Overridden from TrayNotificationView.
  virtual void OnClose() OVERRIDE {
    if (delegate_)
      delegate_->AcceptLocaleChange();
    tray_->HideNotificationView();
  }

 private:
  TrayLocale* tray_;
  LocaleObserver::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LocaleNotificationView);
};


}  // namespace tray

TrayLocale::TrayLocale()
    : TrayImageItem(IDR_AURA_UBER_TRAY_LOCALE),
      notification_(NULL),
      delegate_(NULL) {
}

TrayLocale::~TrayLocale() {
}

bool TrayLocale::GetInitialVisibility() {
  return notification_ != NULL;
}

views::View* TrayLocale::CreateNotificationView(user::LoginStatus status) {
  if (!delegate_)
    return NULL;
  CHECK(notification_ == NULL);
  notification_ = new tray::LocaleNotificationView(
      this, delegate_, cur_locale_, from_locale_, to_locale_);
  return notification_;
}

void TrayLocale::DestroyNotificationView() {
  notification_ = NULL;
}

void TrayLocale::OnLocaleChanged(LocaleObserver::Delegate* delegate,
                                 const std::string& cur_locale,
                                 const std::string& from_locale,
                                 const std::string& to_locale) {
  if (notification_)
    HideNotificationView();
  delegate_ = delegate;
  cur_locale_ = cur_locale;
  from_locale_ = from_locale;
  to_locale_ = to_locale;
  ShowNotificationView();
}

}  // namespace internal
}  // namespace ash
