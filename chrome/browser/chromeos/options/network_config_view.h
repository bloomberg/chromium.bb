// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/cros/network_library.h"
#include "views/window/dialog_delegate.h"

namespace views {
class View;
class Window;
}

namespace chromeos {

class ChildNetworkConfigView;

// A dialog box for showing a password textfield.
class NetworkConfigView : public views::View,
                          public views::DialogDelegate {
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

  // Login dialog for known networks.
  explicit NetworkConfigView(Network* network);
  // Login dialog for new/hidden networks.
  explicit NetworkConfigView(ConnectionType type);
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
  virtual std::wstring GetWindowTitle() const OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

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
  // True when opening in browser, otherwise in OOBE/login mode.
  bool browser_mode_;

  // There's always only one child view, which will get deleted when
  // NetworkConfigView gets cleaned up.
  ChildNetworkConfigView* child_config_view_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigView);
};

// Children of NetworkConfigView must subclass this and implement the virtual
// methods, which are called by NetworkConfigView.
class ChildNetworkConfigView : public views::View {
 public:
  // Called to get title for parent NetworkConfigView dialog box.
  virtual string16 GetTitle() = 0;

  // Called to determine if "Connect" button should be enabled.
  virtual bool CanLogin() = 0;

  // Called when "Connect" button is clicked.
  // Should return false if dialog should remain open.
  virtual bool Login() = 0;

  // Called when "Cancel" button is clicked.
  virtual void Cancel() = 0;

  // Called to set initial focus in a reasonable widget.  Must be done
  // post-construction after the view has a parent window.
  virtual void InitFocus() = 0;

  // Width of passphrase fields.
  static const int kPassphraseWidth;

 protected:
  explicit ChildNetworkConfigView(NetworkConfigView* parent, Network* network)
      : service_path_(network->service_path()),
        parent_(parent) {}
  explicit ChildNetworkConfigView(NetworkConfigView* parent)
      : parent_(parent) {}
  virtual ~ChildNetworkConfigView() {}

  std::string service_path_;
  NetworkConfigView* parent_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildNetworkConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_
