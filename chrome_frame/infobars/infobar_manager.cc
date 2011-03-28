// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/infobars/infobar_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome_frame/infobars/internal/host_window_manager.h"
#include "chrome_frame/infobars/internal/infobar_window.h"

// Connects InfobarWindow to HostWindowManager, and exposes the result as an
// InfobarManager.
class InfobarManagerImpl
    : public InfobarManager,
      public InfobarWindow::Host,
      public HostWindowManager::Delegate {
 public:
  explicit InfobarManagerImpl(HostWindowManager* manager);

  // Implementation of InfobarManager
  virtual bool Show(InfobarContent* content, InfobarType type);
  virtual void Hide(InfobarType type);
  virtual void HideAll();

  // Implementation of HostWindowManager::Delegate
  virtual void AdjustDisplacedWindowDimensions(RECT* rect);

  // Implementation of InfobarWindow::Host
  virtual HWND GetContainerWindow();
  virtual void UpdateLayout();

 private:
  // Not owned by this instance.
  HostWindowManager* manager_;
  // Infobar windows.
  scoped_ptr<InfobarWindow> infobars_[END_OF_INFOBAR_TYPE];
  DISALLOW_COPY_AND_ASSIGN(InfobarManagerImpl);
};  // class InfobarManagerImpl

InfobarManagerImpl::InfobarManagerImpl(HostWindowManager* manager)
    : manager_(manager) {
  for (int index = 0; index < END_OF_INFOBAR_TYPE; ++index) {
    infobars_[index].reset(
        new InfobarWindow(static_cast<InfobarType>(index)));
    infobars_[index]->SetHost(this);
  }
}

bool InfobarManagerImpl::Show(InfobarContent* content, InfobarType type) {
  DCHECK(type >= FIRST_INFOBAR_TYPE && type < END_OF_INFOBAR_TYPE);
  return infobars_[type]->Show(content);
}

void InfobarManagerImpl::Hide(InfobarType type) {
  DCHECK(type >= FIRST_INFOBAR_TYPE && type < END_OF_INFOBAR_TYPE);
  infobars_[type]->Hide();
}

void InfobarManagerImpl::HideAll() {
  for (int index = 0; index < END_OF_INFOBAR_TYPE; ++index)
    Hide(static_cast<InfobarType>(index));
}

void InfobarManagerImpl::AdjustDisplacedWindowDimensions(RECT* rect) {
  for (int index = 0; index < END_OF_INFOBAR_TYPE; ++index) {
    if (infobars_[index] != NULL)
      infobars_[index]->ReserveSpace(rect);
  }
}

HWND InfobarManagerImpl::GetContainerWindow() {
  return *manager_;
}

void InfobarManagerImpl::UpdateLayout() {
  manager_->UpdateLayout();
}

InfobarManager::~InfobarManager() {
}

InfobarManager* InfobarManager::Get(HWND tab_window) {
  HostWindowManager::Delegate* delegate =
      HostWindowManager::GetDelegateForHwnd(tab_window);

  if (delegate != NULL)
    return static_cast<InfobarManagerImpl*>(delegate);

  scoped_ptr<HostWindowManager> host_manager(new HostWindowManager());

  // Transferred to host_manager in call to Initialize.
  InfobarManagerImpl* infobar_manager = new InfobarManagerImpl(
      host_manager.get());

  if (host_manager->Initialize(tab_window, infobar_manager)) {
    host_manager.release();  // takes ownership of itself
    return infobar_manager;
  }

  return NULL;
}
