// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SYNAPTICS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SYNAPTICS_LIBRARY_H_

#include "base/singleton.h"
#include "cros/chromeos_synaptics.h"

namespace chromeos {

// This interface defines interaction with the ChromeOS synaptics library APIs.
// Users can get an instance of this library class like this:
//   SynapticsLibrary::Get()
// For a list of SynapticsPrameters, see chromeos_synaptics.h
// in third_party/cros or /usr/include/cros
class SynapticsLibrary {
 public:
  virtual ~SynapticsLibrary() {}
  // Sets a boolean parameter. The actual call will be run on the FILE thread.
  virtual void SetBoolParameter(SynapticsParameter param, bool value) = 0;

  // Sets a range parameter. The actual call will be run on the FILE thread.
  // Value should be between 1 and 10 inclusive.
  virtual void SetRangeParameter(SynapticsParameter param, int value) = 0;
};


// This class handles the interaction with the ChromeOS synaptics library APIs.
// Users can get an instance of this library class like this:
//   SynapticsLibrary::Get()
// For a list of SynapticsPrameters, see chromeos_synaptics.h
// in third_party/cros or /usr/include/cros
class SynapticsLibraryImpl : public SynapticsLibrary {
 public:
  SynapticsLibraryImpl() {}
  virtual ~SynapticsLibraryImpl() {}

  // SynapticsLibrary overrides.
  virtual void SetBoolParameter(SynapticsParameter param, bool value);
  virtual void SetRangeParameter(SynapticsParameter param, int value);

 private:

  // This helper methods calls into the libcros library to set the parameter.
  // This call is run on the FILE thread.
  void SetParameter(SynapticsParameter param, int value);

  DISALLOW_COPY_AND_ASSIGN(SynapticsLibraryImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SYNAPTICS_LIBRARY_H_
