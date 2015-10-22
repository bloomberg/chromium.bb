// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/global_confirm_info_bar.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/infobars/core/infobar.h"
#include "ui/gfx/image/image.h"

// InfoBarDelegateProxy -------------------------------------------------------

class GlobalConfirmInfoBar::DelegateProxy : public ConfirmInfoBarDelegate {
 public:
  explicit DelegateProxy(base::WeakPtr<GlobalConfirmInfoBar> global_info_bar);
  ~DelegateProxy() override;
  void Detach();

 private:
  friend class GlobalConfirmInfoBar;

  // ConfirmInfoBarDelegate overrides
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;
  void InfoBarDismissed() override;

  infobars::InfoBar* info_bar_;
  base::WeakPtr<GlobalConfirmInfoBar> global_info_bar_;

  DISALLOW_COPY_AND_ASSIGN(DelegateProxy);
};

GlobalConfirmInfoBar::DelegateProxy::DelegateProxy(
    base::WeakPtr<GlobalConfirmInfoBar> global_info_bar)
    : info_bar_(nullptr),
      global_info_bar_(global_info_bar) {
}

GlobalConfirmInfoBar::DelegateProxy::~DelegateProxy() {
}

base::string16 GlobalConfirmInfoBar::DelegateProxy::GetMessageText() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetMessageText()
                          : base::string16();
}

int GlobalConfirmInfoBar::DelegateProxy::GetButtons() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetButtons()
                          : 0;
}

base::string16 GlobalConfirmInfoBar::DelegateProxy::GetButtonLabel(
    InfoBarButton button) const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetButtonLabel(button)
                          : base::string16();
}

bool GlobalConfirmInfoBar::DelegateProxy::Accept() {
  base::WeakPtr<GlobalConfirmInfoBar> info_bar = global_info_bar_;
  if (info_bar)
    info_bar->delegate_->Accept();
  // Could be destroyed after this point.
  if (info_bar)
      info_bar->Close();
  return true;
}

bool GlobalConfirmInfoBar::DelegateProxy::Cancel() {
  base::WeakPtr<GlobalConfirmInfoBar> info_bar = global_info_bar_;
  if (info_bar)
    info_bar->delegate_->Cancel();
  // Could be destroyed after this point.
  if (info_bar)
      info_bar->Close();
  return true;
}

base::string16 GlobalConfirmInfoBar::DelegateProxy::GetLinkText() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetLinkText()
                          : base::string16();
}

GURL GlobalConfirmInfoBar::DelegateProxy::GetLinkURL() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetLinkURL()
                          : GURL();
}

bool GlobalConfirmInfoBar::DelegateProxy::LinkClicked(
    WindowOpenDisposition disposition) {
  return global_info_bar_ ?
      global_info_bar_->delegate_->LinkClicked(disposition) : false;
}

bool GlobalConfirmInfoBar::DelegateProxy::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate == this;
}

void GlobalConfirmInfoBar::DelegateProxy::InfoBarDismissed() {
  base::WeakPtr<GlobalConfirmInfoBar> info_bar = global_info_bar_;
  if (info_bar)
    info_bar->delegate_->InfoBarDismissed();
  // Could be destroyed after this point.
  if (info_bar)
      info_bar->Close();
}

void GlobalConfirmInfoBar::DelegateProxy::Detach() {
  global_info_bar_.reset();
}

// GlobalConfirmInfoBar -------------------------------------------------------

// static
base::WeakPtr<GlobalConfirmInfoBar> GlobalConfirmInfoBar::Show(
    scoped_ptr<ConfirmInfoBarDelegate> delegate) {
  GlobalConfirmInfoBar* info_bar = new GlobalConfirmInfoBar(delegate.Pass());
  return info_bar->weak_factory_.GetWeakPtr();
}

void GlobalConfirmInfoBar::Close() {
  delete this;
}

GlobalConfirmInfoBar::GlobalConfirmInfoBar(
    scoped_ptr<ConfirmInfoBarDelegate> delegate)
    : delegate_(delegate.Pass()),
      browser_tab_strip_tracker_(this, nullptr, nullptr),
      weak_factory_(this) {
   browser_tab_strip_tracker_.Init(
       BrowserTabStripTracker::InitWith::BROWSERS_IN_ACTIVE_DESKTOP);
}

GlobalConfirmInfoBar::~GlobalConfirmInfoBar() {
  while (!proxies_.empty()) {
    auto it = proxies_.begin();
    it->second->Detach();
    it->first->RemoveObserver(this);
    it->first->RemoveInfoBar(it->second->info_bar_);
    proxies_.erase(it);
  }
}

void GlobalConfirmInfoBar::TabInsertedAt(content::WebContents* web_contents,
                                         int index,
                                         bool foreground) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  // WebContents from the tab strip must have the infobar service.
  DCHECK(infobar_service);

  scoped_ptr<GlobalConfirmInfoBar::DelegateProxy> proxy(
      new GlobalConfirmInfoBar::DelegateProxy(weak_factory_.GetWeakPtr()));
  GlobalConfirmInfoBar::DelegateProxy* proxy_ptr = proxy.get();
  infobars::InfoBar* added_bar = infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(proxy.Pass()));

  proxy_ptr->info_bar_ = added_bar;
  DCHECK(added_bar);
  proxies_[infobar_service] = proxy_ptr;
  infobar_service->AddObserver(this);
}

void GlobalConfirmInfoBar::TabChangedAt(content::WebContents* web_contents,
                                        int index,
                                        TabChangeType change_type) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  auto it = proxies_.find(infobar_service);
  if (it == proxies_.end())
    TabInsertedAt(web_contents, index, false);
}

void GlobalConfirmInfoBar::OnInfoBarRemoved(infobars::InfoBar* info_bar,
                                            bool animate) {
  OnManagerShuttingDown(info_bar->owner());
}

void GlobalConfirmInfoBar::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  manager->RemoveObserver(this);
  proxies_.erase(manager);
}
