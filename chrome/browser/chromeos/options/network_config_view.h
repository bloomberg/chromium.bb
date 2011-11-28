// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "ui/gfx/native_widget_types.h"  // gfx::NativeWindow
#include "ui/views/controls/button/button.h"  // views::ButtonListener
#include "ui/views/window/dialog_delegate.h"

namespace views {
class NativeTextButton;
class View;
}

namespace chromeos {

class ChildNetworkConfigView;

// A dialog box for showing a password textfield.
class NetworkConfigView : public views::DialogDelegateView,
                          public views::ButtonListener {
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
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual views::View* GetExtraView() OVERRIDE;

  // views::WidgetDelegate methods.
  virtual bool IsModal() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;

  // views::View overrides.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // views::ButtonListener overrides.
  virtual void ButtonPressed(
      views::Button* sender, const views::Event& event) OVERRIDE;

  void set_delegate(Delegate* delegate) {
    delegate_ = delegate;
  }

 protected:
  // views::View overrides:
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

 private:
  // Creates an "Advanced" button in the lower-left corner of the dialog.
  void CreateAdvancedButton();

  // Resets the underlying view to show advanced options.
  void ShowAdvancedView();

  // There's always only one child view, which will get deleted when
  // NetworkConfigView gets cleaned up.
  ChildNetworkConfigView* child_config_view_;

  Delegate* delegate_;

  // Button in lower-left corner, may be null or hidden.
  views::NativeTextButton* advanced_button_;
  views::View* advanced_button_container_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigView);
};

// Children of NetworkConfigView must subclass this and implement the virtual
// methods, which are called by NetworkConfigView.
class ChildNetworkConfigView : public views::View {
 public:
  ChildNetworkConfigView(NetworkConfigView* parent, Network* network)
      : service_path_(network->service_path()),
        parent_(parent) {}
  explicit ChildNetworkConfigView(NetworkConfigView* parent)
      : parent_(parent) {}
  virtual ~ChildNetworkConfigView() {}

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

  // Minimum with of input fields / combo boxes.
  static const int kInputFieldMinWidth;

 protected:
  std::string service_path_;
  NetworkConfigView* parent_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildNetworkConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONFIG_VIEW_H_
