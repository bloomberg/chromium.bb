// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.annotations.SuppressFBWarnings;

/**
 * Defines API for managing interaction with SmartCard-based certificate storage using PKCS11.
 */
// TODO(changwan): remove this after downstream change is merged.
@SuppressFBWarnings("NM_SAME_SIMPLE_NAME_AS_INTERFACE")
public interface PKCS11AuthenticationManager
        extends org.chromium.chrome.browser.smartcard.PKCS11AuthenticationManager {}