// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Manager for infobar windows.

#ifndef CEEE_IE_PLUGIN_BHO_INFOBAR_MANAGER_H_
#define CEEE_IE_PLUGIN_BHO_INFOBAR_MANAGER_H_

#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "ceee/ie/plugin/bho/infobar_window.h"
#include "ceee/ie/plugin/bho/web_browser_events_source.h"

namespace infobar_api {

// InfobarManager creates and manages infobars, which are displayed at the top
// or bottom of IE content window.
class InfobarManager : public InfobarWindow::Delegate,
                       public WebBrowserEventsSource::Sink {
 public:
  InfobarManager(HWND tab_window, IEventSender* event_sender);

  // Shows the infobar of the specified type and navigates it to the specified
  // URL.
  virtual HRESULT Show(InfobarType type, int max_height,
                       const std::wstring& url, bool slide);
  // Hides the infobar of the specified type.
  virtual HRESULT Hide(InfobarType type);
  // Hides all infobars.
  virtual void HideAll();

  // Implementation of InfobarWindow::Delegate.
  // Finds the handle of the container window.
  virtual HWND GetContainerWindow();
  // Informs about window.close() event.
  virtual void OnWindowClose(InfobarType type);

 protected:
  class ContainerWindow;
  class ContainerWindowInterface {
   public:
    virtual ~ContainerWindowInterface() {}
    virtual bool IsDestroyed() const = 0;
    virtual HWND GetWindowHandle() const = 0;
    virtual bool PostWindowsMessage(UINT msg, WPARAM wparam, LPARAM lparam) = 0;
  };

  // Lazy initialization of InfobarWindow object.
  void LazyInitialize(InfobarType type);

  // Creates container window. Separated for the unit testing.
  virtual ContainerWindowInterface* CreateContainerWindow(
      HWND container, InfobarManager* manager);

  // The HWND of the tab window the infobars are associated with.
  HWND tab_window_;

  // Parent window for IE content window.
  scoped_ptr<ContainerWindowInterface> container_window_;

  // Infobar windows.
  scoped_ptr<InfobarWindow> infobars_[END_OF_INFOBAR_TYPE];

  // Passed to the InfobarWindow during initialization.
  IEventSender* event_sender_;

  // ContainerWindow callbacks.
  // Callback for WM_NCCALCSIZE.
  void OnContainerWindowNcCalcSize(RECT* rect);
  // Callback for messages on size or position change.
  void OnContainerWindowUpdatePosition();
  // Callback for message requesting closing the infobar.
  void OnContainerWindowDelayedCloseInfobar(InfobarType type);
  // Callback for WM_DESTROY.
  void OnContainerWindowDestroy();

  DISALLOW_COPY_AND_ASSIGN(InfobarManager);
};

}  // namespace infobar_api

#endif  // CEEE_IE_PLUGIN_BHO_INFOBAR_MANAGER_H_
