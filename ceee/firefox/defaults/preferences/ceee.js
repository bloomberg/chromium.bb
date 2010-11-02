// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* This file sets default values for CEEE properties.  These properties
 * can be set by the user in the options dialog and are persisted.  This values
 * can also be seen in about:config.
 */

pref("extensions.ceee.last_install_path", "");
pref("extensions.ceee.last_install_time", "");
pref("extensions.ceee.path", "");
pref("extensions.ceee.url", "");
pref("extensions.ceee.debug", false);

// See http://kb.mozillazine.org/Localize_extension_descriptions
pref("extensions.ceee@google.com.description",
     "chrome://ceee/locale/ceee.properties");
