// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_install_prompt_experiment.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kExperimentName[] = "ExtensionPermissionDialog";
const char kGroupPrefix[] = "Group";

// Flags for groups. Not all combinations make sense.
// Refer to the UI screens at http://goo.gl/f2KzPj for those that do.
enum GroupFlag {
  // No changes (Control group).
  NONE = 0,
  // Indicates that the experiment is text only. A text only experiment
  // only adds an explanation text at the bottom of the permission dialog and
  // modifies the text on accept/cancel buttons.
  TEXT_ONLY = 1 << 0,
  // Indicates that the experiment shows inline explanations for permissions.
  INLINE_EXPLANATIONS = 1 << 1,
  // Indicates that the experiment highlights permission text color.
  SHOULD_HIGHLIGHT_TEXT = 1 << 2,
  // Indicates that the experiment highlights permission text background.
  SHOULD_HIGHLIGHT_BACKGROUND = 1 << 3,
  // Indicates that the experiment highlights all permissions.
  SHOULD_HIGHLIGHT_ALL_PERMISSIONS = 1 << 4,
  // Indicates that the experiment puts a "show details" link in the UI.
  SHOULD_SHOW_DETAILS_LINK = 1 << 5,
  // Indicates that the experiment hides the permissions by default and the list
  // can be expanded.
  EXPANDABLE_PERMISSION_LIST = 1 << 6,
  // Indicates that the experiment shows checkboxes for each permission.
  SHOULD_SHOW_CHECKBOXES = 1 << 7
};

// Flags for the actual experiment groups. These flags define what kind of
// UI changes each experiment group does. An experiment group may change
// multiple aspects of the extension install dialog  (e.g. one of the groups
// show a details link and inline explanations for permissions). The control
// group doesn't change the UI. Text only groups add a text warning to the UI,
// with the text changing depending on the group number. Groups with inline
// explanations show detailed explanations for a subset of permissions. Some
// groups highlight the foreground or the background of permission texts.
// The flags reflect the UI screens at http://goo.gl/f2KzPj.
const unsigned int kGroupFlags[] = {
  // Control group doesn't change the UI.
  NONE,
  // Adds "Do you trust this extension to use these privileges safely" text.
  TEXT_ONLY,
  // Adds "Extension can be malicious" text.
  TEXT_ONLY,
  // Adds "Are you sure you want to install" text.
  TEXT_ONLY,
  // Adds "Make sure these privileges make sense for this extension" text.
  TEXT_ONLY,
  // Adds "Do you trust this extension to perform these actions" text.
  TEXT_ONLY,
  // Adds inline explanations displayed by default.
  INLINE_EXPLANATIONS,
  // Adds expandable inline explanations with a "Show Details" link.
  SHOULD_SHOW_DETAILS_LINK | INLINE_EXPLANATIONS,
  // Adds expandable permission list with a "Show Permissions" link.
  SHOULD_SHOW_DETAILS_LINK | EXPANDABLE_PERMISSION_LIST,
  // Highlights text for risky permissions.
  SHOULD_HIGHLIGHT_TEXT,
  // Highlights background for risky permissions.
  SHOULD_HIGHLIGHT_BACKGROUND,
  // Highlights background for all permissions
  SHOULD_HIGHLIGHT_BACKGROUND | SHOULD_HIGHLIGHT_ALL_PERMISSIONS,
  // Displays checkboxes for all permissions.
  SHOULD_SHOW_CHECKBOXES
};

const size_t kGroupCount = arraysize(kGroupFlags);

// Parameters for text only experiments.
const struct TextParams {
  int text_id;
  int ok_text_id;
  int cancel_text_id;
} kTextParams[] = {
  {
    IDS_EXTENSION_PROMPT_EXPERIMENT_EXPLANATION1,
    IDS_EXTENSION_PROMPT_EXPERIMENT_INSTALL_BUTTON_TRUST,
    IDS_CANCEL,
  },
  {
    IDS_EXTENSION_PROMPT_EXPERIMENT_EXPLANATION2,
    IDS_EXTENSION_PROMPT_EXPERIMENT_INSTALL_BUTTON_YES,
    IDS_EXTENSION_PROMPT_EXPERIMENT_INSTALL_BUTTON_NOPE
  },
  {
    IDS_EXTENSION_PROMPT_EXPERIMENT_EXPLANATION3,
    IDS_EXTENSION_PROMPT_EXPERIMENT_INSTALL_BUTTON_SURE,
    IDS_EXTENSION_PROMPT_EXPERIMENT_INSTALL_BUTTON_NOPE
  },
  {
    IDS_EXTENSION_PROMPT_EXPERIMENT_EXPLANATION4,
    IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
    IDS_CANCEL
  },
  {
    IDS_EXTENSION_PROMPT_EXPERIMENT_EXPLANATION5,
    IDS_EXTENSION_PROMPT_EXPERIMENT_INSTALL_BUTTON_TRUST2,
    IDS_EXTENSION_PROMPT_EXPERIMENT_INSTALL_BUTTON_NOPE
  }
};

// Permission warnings in this list have inline explanation texts.
const struct PermissionExplanations {
  int warning_msg_id;
  int extra_explanation_id;
} kPermissionExplanations[] = {
  {
    IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS,
    IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS_EXPLANATION
  },
  {
    IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS,
    IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS_EXPLANATION
  }
};

// Permission warnings in this list are going to be highlighted.
// Note that the matching is done by string comparison, so this list must not
// contain any dynamic strings (e.g. permission for 3 hosts with the host list).
const int kHighlightedWarnings[] = {
  IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS,
  IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS,
  IDS_EXTENSION_PROMPT_WARNING_BOOKMARKS,
  IDS_EXTENSION_PROMPT_WARNING_CONTENT_SETTINGS,
  IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE,
  IDS_EXTENSION_PROMPT_WARNING_INPUT,
  IDS_EXTENSION_PROMPT_WARNING_MANAGEMENT,
  IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ,
  IDS_EXTENSION_PROMPT_WARNING_DEBUGGER
};

bool IsImportantWarning(const base::string16& message) {
  for (size_t i = 0; i < arraysize(kHighlightedWarnings); ++i) {
    if (message == l10n_util::GetStringUTF16(kHighlightedWarnings[i]))
      return true;
  }
  return false;
}

}  // namespace

ExtensionInstallPromptExperiment::ExtensionInstallPromptExperiment(
    unsigned int group_id, unsigned int flags)
    : group_id_(group_id),
      flags_(flags) {
}

ExtensionInstallPromptExperiment::~ExtensionInstallPromptExperiment() {
}

// static
ExtensionInstallPromptExperiment*
    ExtensionInstallPromptExperiment::ControlGroup() {
  return new ExtensionInstallPromptExperiment(0, kGroupFlags[0]);
}

// static
ExtensionInstallPromptExperiment*
    ExtensionInstallPromptExperiment::Find() {
  base::FieldTrial* trial = base::FieldTrialList::Find(kExperimentName);
  // Default is control group.
  unsigned int group_id = 0;
  if (trial) {
    std::vector<std::string> tokens;
    base::SplitString(trial->group_name().c_str(), '_', &tokens);
    if (tokens.size() == 2 && tokens[0] == kGroupPrefix) {
      base::StringToUint(tokens[1], &group_id);
      if (group_id >= kGroupCount)
        group_id = 0;
    }
  }
  return new ExtensionInstallPromptExperiment(group_id, kGroupFlags[group_id]);
}

base::string16 ExtensionInstallPromptExperiment::GetExplanationText() const {
  DCHECK(group_id_ > 0 && group_id_ - 1 < arraysize(kTextParams));
  return l10n_util::GetStringUTF16(kTextParams[group_id_ - 1].text_id);
}

base::string16 ExtensionInstallPromptExperiment::GetOkButtonText() const {
  DCHECK(group_id_ > 0 && group_id_ - 1 < arraysize(kTextParams));
  return l10n_util::GetStringUTF16(kTextParams[group_id_ - 1].ok_text_id);
}

base::string16 ExtensionInstallPromptExperiment::GetCancelButtonText() const {
  DCHECK(group_id_ > 0 && group_id_ - 1 < arraysize(kTextParams));
  return l10n_util::GetStringUTF16(kTextParams[group_id_ - 1].cancel_text_id);
}

bool ExtensionInstallPromptExperiment::text_only() const {
  return (flags_ & TEXT_ONLY) != 0;
}

bool ExtensionInstallPromptExperiment::ShouldHighlightText(
    const base::string16& message) const {
  return (flags_ & SHOULD_HIGHLIGHT_TEXT) != 0 && IsImportantWarning(message);
}

bool ExtensionInstallPromptExperiment::ShouldHighlightBackground(
    const base::string16& message) const {
  return (flags_ & SHOULD_HIGHLIGHT_BACKGROUND) != 0 &&
      ((flags_ & SHOULD_HIGHLIGHT_ALL_PERMISSIONS) != 0 ||
      IsImportantWarning(message));
}

bool ExtensionInstallPromptExperiment::show_details_link() const {
  return (flags_ & SHOULD_SHOW_DETAILS_LINK) != 0;
}

bool ExtensionInstallPromptExperiment::show_checkboxes() const {
  return (flags_ & SHOULD_SHOW_CHECKBOXES) != 0;
}

bool ExtensionInstallPromptExperiment::should_show_expandable_permission_list()
    const {
  return (flags_ & EXPANDABLE_PERMISSION_LIST) != 0;
}

bool ExtensionInstallPromptExperiment::should_show_inline_explanations() const {
  return (flags_ & INLINE_EXPLANATIONS) != 0;
}

base::string16 ExtensionInstallPromptExperiment::GetInlineExplanation(
    const base::string16& message) const {
  for (size_t i = 0; i < arraysize(kPermissionExplanations); ++i) {
    if (message == l10n_util::GetStringUTF16(
        kPermissionExplanations[i].warning_msg_id)) {
      return l10n_util::GetStringUTF16(
          kPermissionExplanations[i].extra_explanation_id);
    }
  }
  return base::string16();
}
