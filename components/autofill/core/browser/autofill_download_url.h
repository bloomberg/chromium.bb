// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DOWNLOAD_URL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DOWNLOAD_URL_H_

class GURL;

namespace autofill {

GURL GetAutofillQueryUrl();
GURL GetAutofillUploadUrl();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DOWNLOAD_URL_H_

