// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_CONIO_POSIX_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_CONIO_POSIX_H_

// Method for disabling buffered IO.
// |true| - disable, |false| - restore previous settings.
void SetTemporaryTermiosSettings(bool temporary);

// Analog from conio.h
int _kbhit();

// Analog from conio.h
int _getche();

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_CONIO_POSIX_H_

