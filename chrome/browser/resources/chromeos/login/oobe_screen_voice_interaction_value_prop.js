// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe Voice Interaction Value Prop screen implementation.
 */

login.createScreen(
    'VoiceInteractionValuePropScreen', 'voice-interaction-value-prop',
    function() {
      return {

        /**
         * Returns a control which should receive an initial focus.
         */
        get defaultControl() {
          return $('voice-interaction-value-prop-md')
              .getElement('continueButton');
        },

        /** @Override */
        onBeforeShow: function(data) {
          var valueView = $('voice-interaction-value-prop-md')
                              .getElement('value-prop-view');

          valueView.addContentScripts([{
            name: 'stripLinks',
            matches: ['<all_urls>'],
            js: {
              code: 'document.querySelectorAll(\'a\').forEach(' +
                  'function(anchor){anchor.href=\'javascript:void(0)\';})'
            },
            run_at: 'document_end'
          }]);

          // TODO(updowndota): provide static content later for the final
          // fallback.
          valueView.request.onHeadersReceived.addListener(function(details) {
            if (details.statusCode == '404') {
              if (valueView.src !=
                  'https://www.gstatic.com/opa-chromeos/oobe/en/value_proposition.html') {
                valueView.src =
                    'https://www.gstatic.com/opa-chromeos/oobe/en/value_proposition.html';
              }
            }
          }, {urls: ['<all_urls>'], types: ['main_frame']});

          var locale = loadTimeData.getString('locale');
          valueView.src = 'https://www.gstatic.com/opa-chromeos/oobe/' +
              locale + '/value_proposition.html';

          Oobe.getInstance().headerHidden = true;
        }
      };
    });
