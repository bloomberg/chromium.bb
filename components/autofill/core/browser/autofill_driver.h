// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_

namespace content {
class WebContents;
}

namespace autofill {

// Interface that provides access to the driver-level context in which Autofill
// is operating. A concrete implementation must be provided by the driver.
class AutofillDriver {
 public:
  virtual ~AutofillDriver() {}

  // TODO(blundell): Remove this method once shared code no longer needs to
  // know about WebContents.
  virtual content::WebContents* GetWebContents() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
