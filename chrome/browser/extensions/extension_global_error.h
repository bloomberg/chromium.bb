// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_GLOBAL_ERROR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_GLOBAL_ERROR_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/common/extensions/extension.h"

class Browser;
class ExtensionService;

// This class encapsulates the UI we want to show users when certain events
// occur related to installed extensions.
class ExtensionGlobalError : public GlobalError {
 public:
  explicit ExtensionGlobalError(ExtensionService* extension_service);
  virtual ~ExtensionGlobalError();

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

  // GlobalError methods.
  virtual bool HasBadge() OVERRIDE;
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual bool HasBubbleView() OVERRIDE;
  virtual string16 GetBubbleViewTitle() OVERRIDE;
  virtual string16 GetBubbleViewMessage() OVERRIDE;
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void OnBubbleViewDidClose(Browser* browser) OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) OVERRIDE;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) OVERRIDE;

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
                                  int template_message_id);

  // Generates the message displayed in the body of the alert.
  string16 GenerateMessage();

  DISALLOW_COPY_AND_ASSIGN(ExtensionGlobalError);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_GLOBAL_ERROR_H_
