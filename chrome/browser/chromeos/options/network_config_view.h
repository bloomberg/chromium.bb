// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_

#include <string>

#include "base/string16.h"
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
class WifiConfigView;

// A dialog box for showing a password textfield.
class NetworkConfigView : public views::View,
                          public views::DialogDelegate,
                          views::TabbedPane::Listener {
 public:
  // Configure dialog for ethernet.
  explicit NetworkConfigView(EthernetNetwork ethernet);
  // Configure dialog for wifi. If |login_only|, then only show login tab.
  explicit NetworkConfigView(WifiNetwork wifi, bool login_only);
  // Configure dialog for cellular.
  explicit NetworkConfigView(CellularNetwork cellular);
  // Login dialog for hidden networks.
  explicit NetworkConfigView();
  virtual ~NetworkConfigView() {}

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
    FLAG_SHOW_WIFI     = 1 << 3,
    FLAG_SHOW_IPCONFIG = 1 << 4,
    FLAG_LOGIN_ONLY    = 1 << 5,
    FLAG_OTHER_NETWORK = 1 << 6,
  };

  // Initializes UI.
  void Init();

  views::TabbedPane* tabs_;

  // NetworkConfigFlags to specify which UIs to show.
  int flags_;

  EthernetNetwork ethernet_;
  WifiNetwork wifi_;
  CellularNetwork cellular_;

  WifiConfigView* wificonfig_view_;
  IPConfigView* ipconfig_view_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_
