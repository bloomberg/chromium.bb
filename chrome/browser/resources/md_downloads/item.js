// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var Item = Polymer({
    is: 'downloads-item',

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

      readyPromise: {
        type: Object,
        value: function() {
          return new Promise(function(resolve, reject) {
            this.resolveReadyPromise_ = resolve;
          }.bind(this));
        },
      },

      scrollbarWidth: {
        type: Number,
        value: 0,
        observer: 'onScrollbarWidthChange_',
      },

      isDangerous_: {
        type: Boolean,
        value: false,
      },

      isIncognito_: {
        type: Boolean,
        value: false,
      },

      /** Only set when |isDangerous| is true. */
      isMalware_: Boolean,
    },

    ready: function() {
      this.content = this.$.content;
      this.resolveReadyPromise_();
    },

    /** @param {!downloads.Data} data */
    update: function(data) {
      assert(!this.data_ || this.data_.id == data.id);

      if (this.data_ &&
          data.by_ext_id === this.data_.by_ext_id &&
          data.by_ext_name === this.data_.by_ext_name &&
          data.danger_type === this.data_.danger_type &&
          data.date_string == this.data_.date_string &&
          data.file_externally_removed == this.data_.file_externally_removed &&
          data.file_name == this.data_.file_name &&
          data.file_path == this.data_.file_path &&
          data.file_url == this.data_.file_url &&
          data.last_reason_text === this.data_.last_reason_text &&
          data.otr == this.data_.otr &&
          data.percent === this.data_.percent &&
          data.progress_status_text === this.data_.progress_status_text &&
          data.received === this.data_.received &&
          data.resume == this.data_.resume &&
          data.retry == this.data_.retry &&
          data.since_string == this.data_.since_string &&
          data.started == this.data_.started &&
          data.state == this.data_.state &&
          data.total == this.data_.total &&
          data.url == this.data_.url) {
        return;
      }

      /** @private {!downloads.Data} */
      this.data_ = data;

      this.isIncognito_ = data.otr;

      // Danger-independent UI and controls.
      var since = data.since_string;
      this.ensureTextIs_(this.$.since, since);
      this.ensureTextIs_(this.$.date, since ? '' : data.date_string);

      /** @const */ var isActive =
          data.state != downloads.States.CANCELLED &&
          data.state != downloads.States.INTERRUPTED &&
          !data.file_externally_removed;
      this.$.content.classList.toggle('is-active', isActive);
      this.$.content.elevation = isActive ? 1 : 0;

      this.ensureTextIs_(this.$.name, data.file_name);
      this.ensureTextIs_(this.$.url, data.url);
      this.$.url.href = data.url;

      // Danger-dependent UI and controls.
      var dangerText = this.getDangerText_(data);
      this.isDangerous_ = !!dangerText;
      this.$.content.classList.toggle('dangerous', this.isDangerous_);

      var description = dangerText || this.getStatusText_(data);

      // Status goes in the "tag" (next to the file name) if there's no file.
      this.ensureTextIs_(this.$.description, isActive ? description : '');
      this.ensureTextIs_(this.$.tag, isActive ? '' : description);

      /** @const */ var showProgress =
          isFinite(data.percent) && !this.isDangerous_;
      this.$.content.classList.toggle('show-progress', showProgress);

      if (showProgress) {
        this.$.progress.indeterminate = data.percent < 0;
        this.$.progress.value = data.percent;
      }

      var hideRemove;

      if (this.isDangerous_) {
        this.isMalware_ =
            data.danger_type == downloads.DangerType.DANGEROUS_CONTENT ||
            data.danger_type == downloads.DangerType.DANGEROUS_HOST ||
            data.danger_type == downloads.DangerType.DANGEROUS_URL ||
            data.danger_type == downloads.DangerType.POTENTIALLY_UNWANTED;
        hideRemove = true;
      } else {
        /** @const */ var completelyOnDisk =
            data.state == downloads.States.COMPLETE &&
            !data.file_externally_removed;

        this.$['file-link'].href = data.url;
        this.ensureTextIs_(this.$['file-link'], data.file_name);

        this.$['file-link'].hidden = !completelyOnDisk;
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

        hideRemove = showCancel ||
            !loadTimeData.getBoolean('allowDeletingHistory');

        /** @const */ var controlledByExtension = data.by_ext_id &&
                                                  data.by_ext_name;
        this.$['controlled-by'].hidden = !controlledByExtension;
        if (controlledByExtension) {
          var link = this.$['controlled-by'].querySelector('a');
          link.href = 'chrome://extensions#' + data.by_ext_id;
          link.textContent = data.by_ext_name;
        }

        var icon = 'chrome://fileicon/' + encodeURIComponent(data.file_path);
        this.iconLoader_.loadScaledIcon(this.$['file-icon'], icon);
      }

      this.$.remove.style.visibility = hideRemove ? 'hidden' : '';
    },

    /**
     * Overwrite |el|'s textContent if it differs from |text|. This is done
     * generally so quickly updating text can be copied via text selection.
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
      this.actionService_.cancel(this.data_.id);
    },

    /**
     * @private
     * @param {Event} e
     */
    onDragStart_: function(e) {
      e.preventDefault();
      this.actionService_.drag(this.data_.id);
    },

    /**
     * @param {Event} e
     * @private
     */
    onFileLinkClick_: function(e) {
      e.preventDefault();
      this.actionService_.openFile(this.data_.id);
    },

    /** @private */
    onPauseClick_: function() {
      this.actionService_.pause(this.data_.id);
    },

    /** @private */
    onRemoveClick_: function() {
      assert(!this.$.remove.disabled);
      this.actionService_.remove(this.data_.id);
    },

    /** @private */
    onSaveDangerous_: function() {
      this.actionService_.saveDangerous(this.data_.id);
    },

    /** @private */
    onDiscardDangerous_: function() {
      this.actionService_.discardDangerous(this.data_.id);
    },

    /** @private */
    onResumeClick_: function() {
      this.actionService_.resume(this.data_.id);
    },

    /** @private */
    onRetryClick_: function() {
      this.actionService_.download(this.$['file-link'].href);
    },

    /** @private */
    onScrollbarWidthChange_: function() {
      if (!this.$)
        return;

      var endCap = this.$['end-cap'];
      endCap.style.flexBasis = '';

      if (this.scrollbarWidth) {
        var basis = parseInt(getComputedStyle(endCap).flexBasis, 10);
        endCap.style.flexBasis = basis - this.scrollbarWidth + 'px';
      }
    },

    /** @private */
    onShowClick_: function() {
      this.actionService_.show(this.data_.id);
    },
  });

  return {Item: Item};
});
