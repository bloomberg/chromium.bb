// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/tray_locale.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_views.h"
#include "base/string16.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace internal {

namespace tray {

class LocaleMessageView : public views::View,
                          public views::LinkListener {
 public:
  LocaleMessageView(LocaleObserver::Delegate* delegate,
                    const std::string& cur_locale,
                    const std::string& from_locale,
                    const std::string& to_locale)
      : delegate_(delegate) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));

    string16 from = l10n_util::GetDisplayNameForLocale(
        from_locale, cur_locale, true);
    string16 to = l10n_util::GetDisplayNameForLocale(
        to_locale, cur_locale, true);

    views::Label* message = new views::Label(
        l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_LOCALE_CHANGE_MESSAGE, from, to));
    message->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    message->SetMultiLine(true);
    message->SizeToFit(kTrayNotificationContentsWidth);
    AddChildView(message);

    views::Link* revert = new views::Link(
        l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_LOCALE_REVERT_MESSAGE, from));
    revert->set_listener(this);
    revert->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    revert->SetMultiLine(true);
    revert->SizeToFit(kTrayNotificationContentsWidth);
    AddChildView(revert);
  }

  virtual ~LocaleMessageView() {}

  // Overridden from views::LinkListener.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE {
    if (delegate_)
      delegate_->RevertLocaleChange();
  }

 private:
  LocaleObserver::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LocaleMessageView);
};

class LocaleNotificationView : public TrayNotificationView {
 public:
  LocaleNotificationView(TrayLocale* tray,
                         LocaleObserver::Delegate* delegate,
                         const std::string& cur_locale,
                         const std::string& from_locale,
                         const std::string& to_locale)
      : TrayNotificationView(tray, IDR_AURA_UBER_TRAY_LOCALE),
        delegate_(delegate) {
    views::View* container = new LocaleMessageView(
        delegate, cur_locale, from_locale, to_locale);
    InitView(container);
  }

  void Update(LocaleObserver::Delegate* delegate,
              const std::string& cur_locale,
              const std::string& from_locale,
              const std::string& to_locale) {
    delegate_ = delegate;
    views::View* container = new LocaleMessageView(
        delegate, cur_locale, from_locale, to_locale);
    UpdateView(container);
  }

  // Overridden from TrayNotificationView.
  virtual void OnClose() OVERRIDE {
    if (delegate_)
      delegate_->AcceptLocaleChange();
  }

 private:
  LocaleObserver::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LocaleNotificationView);
};

}  // namespace tray

TrayLocale::TrayLocale()
    : notification_(NULL),
      delegate_(NULL) {
}

TrayLocale::~TrayLocale() {
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
  delegate_ = delegate;
  cur_locale_ = cur_locale;
  from_locale_ = from_locale;
  to_locale_ = to_locale;
  if (notification_)
    notification_->Update(delegate, cur_locale_, from_locale_, to_locale_);
  else
    ShowNotificationView();
}

}  // namespace internal
}  // namespace ash
