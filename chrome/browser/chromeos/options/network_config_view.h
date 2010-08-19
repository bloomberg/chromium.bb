// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/network_library.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/window/dialog_delegate.h"

namespace views {
class TabbedPane;
class View;
class Window;
}

namespace chromeos {

class IPConfigView;
class CellularConfigView;
class WifiConfigView;

// A dialog box for showing a password textfield.
class NetworkConfigView : public views::View,
                          public views::DialogDelegate,
                          views::TabbedPane::Listener {
 public:
  class Delegate {
   public:
    // Called when dialog "OK" button is pressed.
    virtual void OnDialogAccepted() = 0;

    // Called when dialog "Cancel" button is pressed.
    virtual void OnDialogCancelled() = 0;

   protected:
     virtual ~Delegate() {}
  };

  // Configure dialog for ethernet.
  explicit NetworkConfigView(EthernetNetwork ethernet);
  // Configure dialog for wifi. If |login_only|, then only show login tab.
  explicit NetworkConfigView(WifiNetwork wifi, bool login_only);
  // Configure dialog for cellular.
  explicit NetworkConfigView(CellularNetwork cellular);
  // Login dialog for hidden networks.
  explicit NetworkConfigView();
  virtual ~NetworkConfigView() {}

  // Returns corresponding native window.
  gfx::NativeWindow GetNativeWindow() const;

  // views::DialogDelegate methods.
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool Cancel();
  virtual bool Accept();

  // views::WindowDelegate method.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }

  // views::View overrides.
  virtual std::wstring GetWindowTitle() const;

  // views::TabbedPane::Listener overrides.
  virtual void TabSelectedAt(int index);

  // Sets the focus on the login tab's first textfield.
  void SetLoginTextfieldFocus();

  // Getter/setter for browser mode.
  void set_browser_mode(bool value) {
    browser_mode_ = value;
  }
  bool is_browser_mode() const {
    return browser_mode_;
  }

  void set_delegate(Delegate* delegate) {
    delegate_ = delegate;
  }

 protected:
  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  enum NetworkConfigFlags {
    FLAG_ETHERNET      = 1 << 0,
    FLAG_WIFI          = 1 << 1,
    FLAG_CELLULAR      = 1 << 2,
    FLAG_SHOW_IPCONFIG = 1 << 3,
    FLAG_LOGIN_ONLY    = 1 << 4,
    FLAG_OTHER_NETWORK = 1 << 5,
  };

  // Initializes UI.
  void Init();

  // True when opening in browser, otherwise in OOBE/login mode.
  bool browser_mode_;

  views::TabbedPane* tabs_;

  // NetworkConfigFlags to specify which UIs to show.
  int flags_;

  EthernetNetwork ethernet_;
  WifiNetwork wifi_;
  CellularNetwork cellular_;

  CellularConfigView* cellularconfig_view_;
  WifiConfigView* wificonfig_view_;
  IPConfigView* ipconfig_view_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_
