// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-page' is the settings page containing reset
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-reset-page prefs="{{prefs}}">
 *      </settings-reset-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-reset-page
 */
Polymer({
  is: 'settings-reset-page',


  /** @private */
  onShowDialog_: function() {
     var dialog = document.createElement('settings-reset-profile-dialog');
     this.shadowRoot.appendChild(dialog);
     dialog.open();

     dialog.addEventListener('iron-overlay-closed', function(event) {
       dialog.remove();
     });
  },
});
