// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum.

// This file contains the list of certificate verification results returned
// from Java side to the C++ side.

// Certificate is trusted.
CERT_VERIFY_RESULT_ANDROID(OK, 0)

// Certificate verification could not be conducted.
CERT_VERIFY_RESULT_ANDROID(FAILED, -1)

// Certificate is not trusted due to non-trusted root of the certificate chain.
CERT_VERIFY_RESULT_ANDROID(NO_TRUSTED_ROOT, -2)

// Certificate is not trusted because it has expired.
CERT_VERIFY_RESULT_ANDROID(EXPIRED, -3)

// Certificate is not trusted because it is not valid yet.
CERT_VERIFY_RESULT_ANDROID(NOT_YET_VALID, -4)

// Certificate is not trusted because it could not be parsed.
CERT_VERIFY_RESULT_ANDROID(UNABLE_TO_PARSE, -5)
