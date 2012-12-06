// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_VIRTUAL_DRIVER_VIRTUAL_DRIVER_SWITCHES_H_
#define CLOUD_PRINT_VIRTUAL_DRIVER_VIRTUAL_DRIVER_SWITCHES_H_

namespace switches {
// These constants are duplicated from chrome/common/chrome_switches.cc
// in order to avoid dependency problems.
// TODO(abodenha@chromium.org) Reunify them in some sensible manner.
// Bug: http://crbug.com/88991

// Location of Chrome user profile. Optional.
extern const char kCloudPrintUserDataDir[];

// Used with kCloudPrintFile.  Tells Chrome to delete the file when
// finished displaying the print dialog.
extern const char kCloudPrintDeleteFile[];

// Tells chrome to display the cloud print dialog and upload the
// specified file for printing.
extern const char kCloudPrintFile[];

// Used with kCloudPrintFile to specify a title for the resulting print
// job.
extern const char kCloudPrintJobTitle[];

// Specifies the mime type to be used when uploading data from the
// file referenced by cloud-print-file.
// Defaults to "application/pdf" if unspecified.
extern const char kCloudPrintFileType[];

// Used with kCloudPrintFile to specify a JSON print ticket for the resulting
// print job.
// Defaults to null if unspecified.
extern const char kCloudPrintPrintTicket[];
}  // namespace switches

#endif  // CLOUD_PRINT_VIRTUAL_DRIVER_VIRTUAL_DRIVER_SWITCHES_H_
