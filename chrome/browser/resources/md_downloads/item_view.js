// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var ItemView = Polymer({
    is: 'downloads-item-view',

    /**
     * @param {!downloads.ThrottledIconLoader} iconLoader
     * @param {!downloads.ActionService} actionService
     */
    factoryImpl: function(iconLoader, actionService) {
      /** @private {!downloads.ThrottledIconLoader} */
      this.iconLoader_ = iconLoader;

      /** @private {!downloads.ActionService} */
      this.actionService_ = actionService;
    },

    properties: {
      hideDate: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },

      isDangerous_: {type: Boolean, value: false},

      /** Only set when |isDangerous| is true. */
      isMalware_: Boolean,
    },

    /** @param {!downloads.Data} data */
    update: function(data) {
      assert(!this.id_ || data.id == this.id_);
      this.id_ = data.id;  // This is the only thing saved from |data|.

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

        this.isMalware_ =
            dangerType == downloads.DangerType.DANGEROUS_CONTENT ||
            dangerType == downloads.DangerType.DANGEROUS_HOST ||
            dangerType == downloads.DangerType.DANGEROUS_URL ||
            dangerType == downloads.DangerType.POTENTIALLY_UNWANTED;
      } else {
        var iconUrl = 'chrome://fileicon/' + encodeURIComponent(data.file_path);
        this.iconLoader_.loadScaledIcon(this.$['safe-icon'], iconUrl);

        /** @const */ var noFile =
            data.state == downloads.States.CANCELLED ||
            data.state == downloads.States.INTERRUPTED ||
            data.file_externally_removed;
        this.$.safe.classList.toggle('no-file', noFile);

        /** @const */ var completelyOnDisk =
            data.state == downloads.States.COMPLETE &&
            !data.file_externally_removed;

        this.$['file-link'].href = data.url;
        this.ensureTextIs_(this.$['file-link'], data.file_name);
        this.$['file-link'].hidden = !completelyOnDisk;

        this.ensureTextIs_(this.$.name, data.file_name);
        this.$.name.hidden = completelyOnDisk;

        this.$.show.hidden = !completelyOnDisk;

        this.$.retry.hidden = !data.retry;

        /** @const */ var isInProgress =
            data.state == downloads.States.IN_PROGRESS;
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

        // TODO(dbeam): "Cancelled" should show status next to the file name.
        this.ensureTextIs_(this.$.status, this.getStatusText_(data));

        /** @const */ var hasPercent = isFinite(data.percent);
        this.$.progress.hidden = !hasPercent;

        if (hasPercent) {
          this.$.progress.indeterminate = data.percent < 0;
          this.$.progress.value = data.percent;
        }
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

    /** @private */
    onCancelClick_: function() {
      this.actionService_.cancel(this.id_);
    },

    /** @private */
    onDangerousRemoveOrDiscardClick_: function() {
      this.actionService_.discardDangerous(this.id_);
    },

    /**
     * @private
     * @param {Event} e
     */
    onDragStart_: function(e) {
      e.preventDefault();
      this.actionService_.drag(this.id_);
    },

    /**
     * @param {Event} e
     * @private
     */
    onFileLinkClick_: function(e) {
      e.preventDefault();
      this.actionService_.openFile(this.id_);
    },

    /** @private */
    onPauseClick_: function() {
      this.actionService_.pause(this.id_);
    },

    /** @private */
    onRemoveClick_: function() {
      this.actionService_.remove(this.id_);
    },

    /** @private */
    onRestoreOrSaveClick_: function() {
      this.actionService_.saveDangerous(this.id_);
    },

    /** @private */
    onResumeClick_: function() {
      this.actionService_.resume(this.id_);
    },

    onRetryClick_: function() {
      this.actionService_.download(this.$['file-link'].href);
    },

    /** @private */
    onShowClick_: function() {
      this.actionService_.show(this.id_);
    },
  });

  return {ItemView: ItemView};
});
