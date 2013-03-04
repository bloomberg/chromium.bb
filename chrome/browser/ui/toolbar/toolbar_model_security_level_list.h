// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Intentionally no include guards because this file is meant to be included
// inside a macro to generate enum values.

#ifndef DEFINE_TOOLBAR_MODEL_SECURITY_LEVEL
#error "Please define DEFINE_TOOLBAR_MODEL_SECURITY_LEVEL before including \
  this file."
#endif

// HTTP/no URL/user is editing
DEFINE_TOOLBAR_MODEL_SECURITY_LEVEL(NONE, 0)

// HTTPS with valid EV cert
DEFINE_TOOLBAR_MODEL_SECURITY_LEVEL(EV_SECURE, 1)

// HTTPS (non-EV)
DEFINE_TOOLBAR_MODEL_SECURITY_LEVEL(SECURE, 2)

// HTTPS, but unable to check certificate revocation status or with insecure
// content on the page
DEFINE_TOOLBAR_MODEL_SECURITY_LEVEL(SECURITY_WARNING, 3)

// Attempted HTTPS and failed, page not authenticated
DEFINE_TOOLBAR_MODEL_SECURITY_LEVEL(SECURITY_ERROR, 4)

DEFINE_TOOLBAR_MODEL_SECURITY_LEVEL(NUM_SECURITY_LEVELS, 5)
