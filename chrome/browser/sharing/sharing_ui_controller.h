// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "ui/gfx/image/image.h"

class BrowserWindow;
class SharingDeviceInfo;
class SharingDialog;
class SharingService;

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace content {
class WebContents;
}  // namespace content

// The controller for desktop dialog with the list of synced devices and apps.
class SharingUiController {
 public:
  struct App {
    App(const gfx::VectorIcon* vector_icon,
        const gfx::Image& image,
        base::string16 name,
        std::string identifier);
    App(App&& other);
    ~App();

    const gfx::VectorIcon* vector_icon = nullptr;
    gfx::Image image;
    base::string16 name;
    std::string identifier;
  };

  explicit SharingUiController(content::WebContents* web_contents);
  virtual ~SharingUiController();

  // Title of the dialog.
  virtual base::string16 GetTitle() = 0;

  // Returns filtered list of synced devices for the feature.
  virtual std::vector<SharingDeviceInfo> GetSyncedDevices();

  // Returns list of local apps capable of supporting action.
  virtual std::vector<App> GetApps() = 0;

  // Called when user chooses a synced device to complete the task.
  virtual void OnDeviceChosen(const SharingDeviceInfo& device) = 0;

  // Called when user chooses a local app to complete the task.
  virtual void OnAppChosen(const App& app) = 0;

  virtual PageActionIconType GetIconType() = 0;

  virtual int GetRequiredDeviceCapabilities() = 0;

  // Called by the SharingDialog when it is being closed.
  void OnDialogClosed(SharingDialog* dialog);

  void InvalidateOldDialog();

  // Returns the currently open SharingDialog or nullptr if there is no
  // dialog open.
  SharingDialog* dialog() const { return dialog_; }
  bool is_loading() const { return is_loading_; }
  bool send_failed() const { return send_failed_; }
  content::WebContents* web_contents() const { return web_contents_; }

 protected:
  virtual SharingDialog* DoShowDialog(BrowserWindow* window) = 0;

  void SendMessageToDevice(
      const SharingDeviceInfo& device,
      chrome_browser_sharing::SharingMessage sharing_message);

 private:
  // Updates the omnibox icon if available.
  void UpdateIcon();
  // Shows a new SharingDialog and closes the old one.
  void ShowNewDialog();

  // Called after a message got sent to a device. Shows a new error dialog if
  // |success| is false and updates the omnibox icon.
  void OnMessageSentToDevice(int dialog_id, bool success);

  SharingDialog* dialog_ = nullptr;
  content::WebContents* web_contents_ = nullptr;
  SharingService* sharing_service_ = nullptr;

  bool is_loading_ = false;
  bool send_failed_ = false;

  // ID of the last shown dialog used to ignore events from old dialogs.
  int last_dialog_id_ = 0;

  base::WeakPtrFactory<SharingUiController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_
