// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Provides the UI for dump creation.
 */
var DumpCreator = (function() {
  /**
   * @param {Element} containerElement The parent element of the dump creation
   *     UI.
   * @constructor
   */
  function DumpCreator(containerElement) {
    /**
     * The root element of the dump creation UI.
     * @type {Element}
     * @private
     */
    this.root_ = document.createElement('details');

    this.root_.className = 'peer-connection-dump-root';
    containerElement.appendChild(this.root_);
    var summary = document.createElement('summary');
    this.root_.appendChild(summary);
    summary.textContent = 'Create Dump';
    var content = document.createElement('div');
    this.root_.appendChild(content);

    content.innerHTML = '<div><a><button>' +
        'Download the PeerConnection updates and stats data' +
        '</button></a></div>' +
        '<p><label><input type=checkbox>' +
        'Enable diagnostic audio recordings.</label></p>' +
        '<p>A diagnostic audio recording is used for analyzing audio' +
        ' problems. It contains the audio played out from the speaker and' +
        ' recorded from the microphone and is saved to the local disk.' +
        ' Checking this box will enable the recording for an ongoing WebRTC' +
        ' call and for future WebRTC calls. When the box is unchecked or this' +
        ' page is closed, this recording functionality will be disabled for' +
        ' future WebRTC calls, but an ongoing call will continue to record' +
        ' until the call is ended. Only recording in one tab is supported.' +
        ' If several tabs are running WebRTC calls, the resulting file will' +
        ' be invalid. To restart the dump, the tab with the call being' +
        ' recorded must be closed and recording disabled and enabled again.' +
        ' When enabling, you select a file to save the dump to. Choose a' +
        ' non-existing file name. Selecting an existing file will append to' +
        ' it, not overwrite it, rendering the file invalid. </p>';

    content.getElementsByTagName('a')[0].addEventListener(
        'click', this.onDownloadData_.bind(this));
    content.getElementsByTagName('input')[0].addEventListener(
        'click', this.onAecRecordingChanged_.bind(this));
  }

  DumpCreator.prototype = {
    /** Mark the AEC recording checkbox checked.*/
    enableAecRecording: function() {
      this.root_.getElementsByTagName('input')[0].checked = true;
    },

    /**
     * Downloads the PeerConnection updates and stats data as a file.
     *
     * @private
     */
    onDownloadData_: function() {
      var dump_object =
      {
        'getUserMedia': userMediaRequests,
        'PeerConnections': peerConnectionDataStore,
      };
      var textBlob = new Blob([JSON.stringify(dump_object, null, ' ')],
                              {type: 'octet/stream'});
      var URL = window.URL.createObjectURL(textBlob);

      this.root_.getElementsByTagName('a')[0].href = URL;
      // The default action of the anchor will download the URL.
    },

    /**
     * Handles the event of toggling the AEC recording state.
     *
     * @private
     */
    onAecRecordingChanged_: function() {
      var enabled = this.root_.getElementsByTagName('input')[0].checked;
      if (enabled) {
        chrome.send('enableAecRecording');
      } else {
        chrome.send('disableAecRecording');
      }
    },
  };
  return DumpCreator;
})();
