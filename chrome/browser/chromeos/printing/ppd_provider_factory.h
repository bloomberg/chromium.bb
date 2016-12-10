// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PPD_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PPD_PROVIDER_FACTORY_H_

#include <memory>

class Profile;

namespace chromeos {
namespace printing {

class PpdProvider;

std::unique_ptr<PpdProvider> CreateProvider(Profile* profile);

}  // namespace printing
}  // namsepace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PPD_PROVIDER_FACTORY_H_
