// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This files declares a class that contains methods and data to conduct
// for user expeirments.

#ifndef CHROME_INSTALLER_UTIL_USER_EXPERIMENT_H_
#define CHROME_INSTALLER_UTIL_USER_EXPERIMENT_H_

#include "base/strings/string16.h"
#include "chrome/installer/util/util_constants.h"

namespace base {
class CommandLine;
class FilePath;
}

namespace installer {

class Product;

// Flags to control what to show in the UserExperiment dialog.
enum ToastUiFlags {
  kToastUiUninstall          = 1 << 0,  // Uninstall radio button.
  kToastUiDontBugMeAsButton  = 1 << 1,  // This is a button, not a radio button.
  kToastUiWhyLink            = 1 << 2,  // Has the 'why I am seeing this' link.
  kToastUiMakeDefault        = 1 << 3,  // Has the 'make it default' checkbox.
};

// A struct for communicating what a UserExperiment contains. In these
// experiments we show toasts to the user if they are inactive for a certain
// amount of time.
struct ExperimentDetails {
  base::string16 prefix;  // The experiment code prefix for this experiment,
                          // also known as the 'TV' part in 'TV80'.
  int flavor;           // The flavor index for this experiment.
  int heading;          // The heading resource ID to use for this experiment.
  int flags;            // See ToastUIFlags above.
  int control_group;    // Size of the control group (in percentages). Control
                        // group is the group that qualifies for the
                        // experiment but does not participate.
};

// Creates the experiment details for a given language-brand combo.
// If |flavor| is -1, then a flavor will be selected at random. |experiment|
// is the struct you want to write the experiment information to.
// Returns false if no experiment details could be gathered.
bool CreateExperimentDetails(int flavor, ExperimentDetails* experiment);

// After an install or upgrade the user might qualify to participate in an
// experiment. This function determines if the user qualifies and if so it
// sets the wheels in motion or in simple cases does the experiment itself.
void LaunchBrowserUserExperiment(const base::CommandLine& base_command,
                                 InstallStatus status,
                                 bool system_level);

// The user has qualified for the inactive user toast experiment and this
// function just performs it.
void InactiveUserToastExperiment(int flavor,
                                 const base::string16& experiment_group,
                                 const Product& product,
                                 const base::FilePath& application_path);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_USER_EXPERIMENT_H_
