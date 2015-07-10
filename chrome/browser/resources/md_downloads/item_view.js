// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var ItemView = Polymer({
    is: 'item-view',

    /** @param {!downloads.ThrottledIconLoader} iconLoader */
    factoryImpl: function(iconLoader) {
      /** @private {!downloads.ThrottledIconLoader} */
      this.iconLoader_ = iconLoader;
    },

    properties: {
      hideDate: {type: Boolean, value: false},
      isDangerous: {type: Boolean, value: false},
      // Only use |isMalware| if |isDangerous| is true.
      isMalware: Boolean,
    },

    ready: function() {
      this.$.safe.ondragstart = this.onSafeDragstart_.bind(this);
      this.$['file-link'].onclick = this.onFileLinkClick_.bind(this);
      this.$.show.onclick = this.onShowClick_.bind(this);
      this.$.pause.onclick = this.onPauseClick_.bind(this);
      this.$.resume.onclick = this.onResumeClick_.bind(this);
      this.$['safe-remove'].onclick = this.onSafeRemoveClick_.bind(this);
      this.$.cancel.onclick = this.onCancelClick_.bind(this);
      this.$.restore.onclick = this.onRestoreClick_.bind(this);
      this.$.save.onclick = this.onSaveClick_.bind(this);
      this.$['dangerous-remove'].onclick = this.onDangerRemoveClick_.bind(this);
      this.$.discard.onclick = this.onDiscardClick_.bind(this);
    },

    /** @param {!downloads.Data} data */
    update: function(data) {
      assert(!this.id_ || data.id == this.id_);
      this.id_ = data.id;  // This is the only thing saved from |data|.

      this.classList.toggle('otr', data.otr);

      this.ensureTextIs_(this.$.since, data.since_string);
      this.ensureTextIs_(this.$.date, data.date_string);

      var dangerText = this.getDangerText_(data);
      this.isDangerous = !!dangerText;

      if (dangerText) {
        this.ensureTextIs_(this.$.description, dangerText);

        var dangerType = data.danger_type;
        var dangerousFile = dangerType == downloads.DangerType.DANGEROUS_FILE;
        this.$.description.classList.toggle('malware', !dangerousFile);

        var idr = dangerousFile ? 'IDR_WARNING' : 'IDR_SAFEBROWSING_WARNING';
        var iconUrl = 'chrome://theme/' + idr;
        this.iconLoader_.loadScaledIcon(this.$['dangerous-icon'], iconUrl);

        this.isMalware =
            dangerType == downloads.DangerType.DANGEROUS_CONTENT ||
            dangerType == downloads.DangerType.DANGEROUS_HOST ||
            dangerType == downloads.DangerType.DANGEROUS_URL ||
            dangerType == downloads.DangerType.POTENTIALLY_UNWANTED;
      } else {
        var iconUrl = 'chrome://fileicon/' + encodeURIComponent(data.file_path);
        this.iconLoader_.loadScaledIcon(this.$['safe-icon'], iconUrl);

        /** @const */ var isInProgress =
            data.state == downloads.States.IN_PROGRESS;
        this.classList.toggle('in-progress', isInProgress);

        /** @const */ var completelyOnDisk =
            data.state == downloads.States.COMPLETE &&
            !data.file_externally_removed;

        this.$['file-link'].href = data.url;
        this.ensureTextIs_(this.$['file-link'], data.file_name);
        this.$['file-link'].hidden = !completelyOnDisk;

        /** @const */ var isInterrupted =
            data.state == downloads.States.INTERRUPTED;
        this.$.name.classList.toggle('interrupted', isInterrupted);
        this.ensureTextIs_(this.$.name, data.file_name);
        this.$.name.hidden = completelyOnDisk;

        this.$.show.hidden = !completelyOnDisk;

        this.$.retry.href = data.url;
        this.$.retry.hidden = !data.retry;

        this.$.pause.hidden = !isInProgress;

        this.$.resume.hidden = !data.resume;

        /** @const */ var isPaused = data.state == downloads.States.PAUSED;
        /** @const */ var showCancel = isPaused || isInProgress;
        this.$.cancel.hidden = !showCancel;

        this.$['safe-remove'].hidden = showCancel ||
            !loadTimeData.getBoolean('allowDeletingHistory');

        /** @const */ var controlledByExtension = data.by_ext_id &&
                                                  data.by_ext_name;
        this.$['controlled-by'].hidden = !controlledByExtension;
        if (controlledByExtension) {
          var link = this.$['controlled-by'].querySelector('a');
          link.href = 'chrome://extensions#' + data.by_ext_id;
          link.setAttribute('column-type', 'controlled-by');
          link.textContent = data.by_ext_name;
        }

        this.ensureTextIs_(this.$['src-url'], data.url);
        this.$['src-url'].href = data.url;
        this.ensureTextIs_(this.$.status, this.getStatusText_(data));

        this.$.progress.hidden = !isInProgress;

        if (isInProgress)
          this.$.progress.value = data.percent;
      }
    },

    /**
     * Overwrite |el|'s textContent if it differs from |text|.
     * @param {!Element} el
     * @param {string} text
     * @private
     */
    ensureTextIs_: function(el, text) {
      if (el.textContent != text)
        el.textContent = text;
    },

    /**
     * @param {!downloads.Data} data
     * @return {string} Text describing the danger of a download. Empty if not
     *     dangerous.
     */
    getDangerText_: function(data) {
      switch (data.danger_type) {
        case downloads.DangerType.DANGEROUS_FILE:
          return loadTimeData.getStringF('dangerFileDesc', data.file_name);
        case downloads.DangerType.DANGEROUS_URL:
          return loadTimeData.getString('dangerUrlDesc');
        case downloads.DangerType.DANGEROUS_CONTENT:  // Fall through.
        case downloads.DangerType.DANGEROUS_HOST:
          return loadTimeData.getStringF('dangerContentDesc', data.file_name);
        case downloads.DangerType.UNCOMMON_CONTENT:
          return loadTimeData.getStringF('dangerUncommonDesc', data.file_name);
        case downloads.DangerType.POTENTIALLY_UNWANTED:
          return loadTimeData.getStringF('dangerSettingsDesc', data.file_name);
        default:
          return '';
      }
    },

    /**
     * @param {!downloads.Data} data
     * @return {string} User-visible status update text.
     * @private
     */
    getStatusText_: function(data) {
      switch (data.state) {
        case downloads.States.IN_PROGRESS:
        case downloads.States.PAUSED:  // Fallthrough.
          assert(typeof data.progress_status_text == 'string');
          return data.progress_status_text;
        case downloads.States.CANCELLED:
          return loadTimeData.getString('statusCancelled');
        case downloads.States.DANGEROUS:
          break;  // Intentionally hit assertNotReached(); at bottom.
        case downloads.States.INTERRUPTED:
          assert(typeof data.last_reason_text == 'string');
          return data.last_reason_text;
        case downloads.States.COMPLETE:
          return data.file_externally_removed ?
              loadTimeData.getString('statusRemoved') : '';
      }
      assertNotReached();
      return '';
    },

    /**
     * @private
     * @param {Event} e
     */
    onSafeDragstart_: function(e) {
      e.preventDefault();
      chrome.send('drag', [this.id_]);
    },

    /**
     * @param {Event} e
     * @private
     */
    onFileLinkClick_: function(e) {
      e.preventDefault();
      chrome.send('openFile', [this.id_]);
    },

    /** @private */
    onShowClick_: function() {
      chrome.send('show', [this.id_]);
    },

    /** @private */
    onPauseClick_: function() {
      chrome.send('pause', [this.id_]);
    },

    /** @private */
    onResumeClick_: function() {
      chrome.send('resume', [this.id_]);
    },

    /** @private */
    onSafeRemoveClick_: function() {
      chrome.send('remove', [this.id_]);
    },

    /** @private */
    onCancelClick_: function() {
      chrome.send('cancel', [this.id_]);
    },

    /** @private */
    onRestoreClick_: function() {
      this.onSaveClick_();
    },

    /** @private */
    onSaveClick_: function() {
      chrome.send('saveDangerous', [this.id_]);
    },

    /** @private */
    onDangerRemoveClick_: function() {
      this.onDiscardClick_();
    },

    /** @private */
    onDiscardClick_: function() {
      chrome.send('discardDangerous', [this.id_]);
    },
  });

  return {ItemView: ItemView};
});
