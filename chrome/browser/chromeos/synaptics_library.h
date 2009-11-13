// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYNAPTICS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_SYNAPTICS_LIBRARY_H_

#include "base/singleton.h"
#include "third_party/cros/chromeos_synaptics.h"

namespace chromeos {

// This class handles the interaction with the ChromeOS synaptics library APIs.
// Users can get an instance of this library class like this:
//   SynapticsLibrary::Get()
// For a list of SynapticsPrameters, see third_party/cros/chromeos_synaptics.h
class SynapticsLibrary {
 public:
  // This gets the singleton SynapticsLibrary.
  static SynapticsLibrary* Get();

  // Makes sure the library is loaded, loading it if necessary. Returns true if
  // the library has been successfully loaded.
  static bool EnsureLoaded();

  // Sets a boolean parameter. The actual call will be run on the FILE thread.
  void SetBoolParameter(SynapticsParameter param, bool value);

  // Sets a range parameter. The actual call will be run on the FILE thread.
  // Value should be between 1 and 10 inclusive.
  void SetRangeParameter(SynapticsParameter param, int value);

 private:
  friend struct DefaultSingletonTraits<SynapticsLibrary>;

  SynapticsLibrary() {}
  ~SynapticsLibrary() {}

  // This helper methods calls into the libcros library to set the parameter.
  // This call is run on the FILE thread.
  void SetParameter(SynapticsParameter param, int value);

  DISALLOW_COPY_AND_ASSIGN(SynapticsLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYNAPTICS_LIBRARY_H_
