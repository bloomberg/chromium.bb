// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_EXPERIMENT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_EXPERIMENT_H_

#include "base/memory/ref_counted.h"

// Represents a permission dialog experiment.
// TODO(meacer): Remove this class once the ExtensionPermissionDialog
// experiment is completed (http://crbug.com/308748).
class ExtensionInstallPromptExperiment
    : public base::RefCounted<ExtensionInstallPromptExperiment> {
 public:
  ExtensionInstallPromptExperiment(unsigned int group_id, unsigned int flags);

  // Returns an experiment instance configured by the server. The ownership of
  // the returned pointer is passed to the caller.
  static ExtensionInstallPromptExperiment* Find();

  // Returns an experiment instance for the control group. The ownership of the
  // returned pointer is passed to the caller.
  static ExtensionInstallPromptExperiment* ControlGroup();

  // Returns true if this is a text only experiment. A text only experiment
  // only adds an explanation text at the bottom of the permission dialog
  // and changes the text on the add/cancel buttons.
  bool text_only() const;

  // The explanation text to be added for text only experiments.
  base::string16 GetExplanationText() const;

  // The text for the accept button for text only experiments.
  base::string16 GetOkButtonText() const;

  // The text for the cancel button for text only experiments.
  base::string16 GetCancelButtonText() const;

  // Returns true if the text color should be highlighted for the given
  // permission message.
  bool ShouldHighlightText(const base::string16& message) const;

  // Returns true if the text background should be highlighted for the given
  // permission message.
  bool ShouldHighlightBackground(const base::string16& message) const;

  // Returns true if there should be a "Show details" link at the bottom of the
  // permission dialog.
  bool show_details_link() const;

  // Returns true if there should be checkboxes next to permissions for the
  // user to click.
  bool show_checkboxes() const;

  // Returns true if the permission list should be hidden by default and can
  // be expanded when necessary.
  bool should_show_expandable_permission_list() const;

  // Returns true if the experiment should show inline explanations for
  // permissions.
  bool should_show_inline_explanations() const;

  // Returns the inline explanation text for the given permission warning.
  // Returns empty string if there is no corresponding inline explanation.
  base::string16 GetInlineExplanation(const base::string16& message) const;

 private:
  friend class base::RefCounted<ExtensionInstallPromptExperiment>;
  ~ExtensionInstallPromptExperiment();

  // Group id of the experiment. The zeroth group is the control group.
  const unsigned int group_id_;
  // Bitmask for the changes done to the UI by the experiment. An experiment can
  // change multiple parts of the UI.
  const unsigned int flags_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallPromptExperiment);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_EXPERIMENT_H_
