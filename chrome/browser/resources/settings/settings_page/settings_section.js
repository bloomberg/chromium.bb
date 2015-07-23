// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-section' shows a paper material themed section with a header
 * which shows its page title and icon.
 *
 * Example:
 *
 *    <cr-settings-section page-title="[[pageTitle]]" icon="[[icon]]">
 *      <!-- Insert your section controls here -->
 *    </cr-settings-section>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-section
 */
Polymer({
  is: 'cr-settings-section',

  properties: {
    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: String,

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: String,
  },
});
