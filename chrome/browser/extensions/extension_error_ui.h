// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/common/extensions/extension.h"

class Browser;
class ExtensionService;

// This class encapsulates the UI we want to show users when certain events
// occur related to installed extensions.
class ExtensionErrorUI {
 public:
  static ExtensionErrorUI* Create(ExtensionService* extension_service);

  virtual ~ExtensionErrorUI();

  // Inform us that a given extension is of a certain type that the user
  // hasn't yet acknowledged.
  void AddExternalExtension(const std::string& id);
  void AddBlacklistedExtension(const std::string& id);
  void AddOrphanedExtension(const std::string& id);

  // Returns sets replaying the IDs that have been added with the
  // Add[...]Extension methods.
  const extensions::ExtensionIdSet* get_external_extension_ids() const {
    return external_extension_ids_.get();
  }

  const extensions::ExtensionIdSet* get_blacklisted_extension_ids() const {
    return blacklisted_extension_ids_.get();
  }

  const extensions::ExtensionIdSet* get_orphaned_extension_ids() const {
    return orphaned_extension_ids_.get();
  }

  // Shows the installation error in a bubble view. Should return true if a
  // bubble is shown, false if one could not be shown.
  virtual bool ShowErrorInBubbleView() = 0;

  // Shows the extension page. Called as a result of the user clicking more
  // info and should be only called from the context of a callback
  // (BubbleViewDidClose or BubbleViewAccept/CancelButtonPressed).
  // It should use the same browser as where the bubble was shown.
  virtual void ShowExtensions() = 0;

 protected:
  explicit ExtensionErrorUI(ExtensionService* extension_service);

  ExtensionService* extension_service() const { return extension_service_; }

  // Model methods for the bubble view.
  string16 GetBubbleViewTitle();
  string16 GetBubbleViewMessage();
  string16 GetBubbleViewAcceptButtonLabel();
  string16 GetBubbleViewCancelButtonLabel();

  // Sub-classes should call this methods based on the actions taken by the user
  // in the error bubble.
  void BubbleViewDidClose();
  void BubbleViewAcceptButtonPressed();
  void BubbleViewCancelButtonPressed();

 private:
  bool should_delete_self_on_close_;
  ExtensionService* extension_service_;
  scoped_ptr<extensions::ExtensionIdSet> external_extension_ids_;
  scoped_ptr<extensions::ExtensionIdSet> blacklisted_extension_ids_;
  scoped_ptr<extensions::ExtensionIdSet> orphaned_extension_ids_;
  string16 message_;  // Displayed in the body of the alert.

  // For a given set of extension IDs, generates appropriate text
  // describing what the user needs to know about them.
  string16 GenerateMessageSection(const extensions::ExtensionIdSet* extensions,
                                  int extension_template_message_id,
                                  int app_template_message_id);

  // Generates the message displayed in the body of the alert.
  string16 GenerateMessage();

  DISALLOW_COPY_AND_ASSIGN(ExtensionErrorUI);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_H_
