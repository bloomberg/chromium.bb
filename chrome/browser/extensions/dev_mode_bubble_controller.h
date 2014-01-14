// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DEV_MODE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_DEV_MODE_BUBBLE_CONTROLLER_H_

#include <string>
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/extension.h"

class Browser;
class ExtensionService;

using extensions::ExtensionMessageBubbleController;

namespace {

class DevModeBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  explicit DevModeBubbleDelegate(ExtensionService* service);
  virtual ~DevModeBubbleDelegate();

  // ExtensionMessageBubbleController::Delegate methods.
  virtual bool ShouldIncludeExtension(const std::string& extension_id) OVERRIDE;
  virtual void AcknowledgeExtension(
      const std::string& extension_id,
      ExtensionMessageBubbleController::BubbleAction user_action) OVERRIDE;
  virtual void PerformAction(const extensions::ExtensionIdList& list) OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual base::string16 GetMessageBody() const OVERRIDE;
  virtual base::string16 GetOverflowText(
      const base::string16& overflow_count) const OVERRIDE;
  virtual base::string16 GetLearnMoreLabel() const OVERRIDE;
  virtual GURL GetLearnMoreUrl() const OVERRIDE;
  virtual base::string16 GetActionButtonLabel() const OVERRIDE;
  virtual base::string16 GetDismissButtonLabel() const OVERRIDE;
  virtual bool ShouldShowExtensionList() const OVERRIDE;
  virtual void LogExtensionCount(size_t count) OVERRIDE;
  virtual void LogAction(
      ExtensionMessageBubbleController::BubbleAction action) OVERRIDE;

 private:
  // Our extension service. Weak, not owned by us.
  ExtensionService* service_;

  DISALLOW_COPY_AND_ASSIGN(DevModeBubbleDelegate);
};

}  // namespace

namespace extensions {

class DevModeBubble;

class DevModeBubbleController
    : public ExtensionMessageBubbleController {
 public:
  // Clears the list of profiles the bubble has been shown for. Should only be
  // used during testing.
  static void ClearProfileListForTesting();

  // Returns true if the extension is considered a Developer Mode extension.
  static bool IsDevModeExtension(const Extension* extension);

  explicit DevModeBubbleController(Profile* profile);
  virtual ~DevModeBubbleController();

  // Whether the controller knows of extensions to list in the bubble. Returns
  // true if so.
  bool ShouldShow();

  // ExtensionMessageBubbleController methods.
  virtual void Show(ExtensionMessageBubble* bubble) OVERRIDE;

 private:
  // A weak pointer to the profile we are associated with. Not owned by us.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(DevModeBubbleController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DEV_MODE_BUBBLE_CONTROLLER_H_
