// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior to determine whether a pref is controlled by policy.
 */

/** @polymerBehavior */
var PolicyControllable = {
  /**
   * @param {?chrome.settingsPrivate.PrefObject} pref
   * @return {boolean} True if the pref is controlled by an enforced policy.
   */
  isPolicyControlled: function(pref) {
    return !!pref &&
            pref.policyEnforcement ==
                chrome.settingsPrivate.PolicyEnforcement.ENFORCED;
  },
};
