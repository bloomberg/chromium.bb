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
        value: true,
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
        observer: 'onScrollbarWidthChange_',
        type: Number,
        value: 0,
      },

      completelyOnDisk_: {
        computed: 'computeCompletelyOnDisk_(' +
            'data_.state, data_.file_externally_removed)',
        type: Boolean,
        value: true,
      },

      i18n_: {
        readOnly: true,
        type: Object,
        value: function() {
          return {
            cancel: loadTimeData.getString('controlCancel'),
            discard: loadTimeData.getString('dangerDiscard'),
            pause: loadTimeData.getString('controlPause'),
            remove: loadTimeData.getString('controlRemoveFromList'),
            resume: loadTimeData.getString('controlResume'),
            restore: loadTimeData.getString('dangerRestore'),
            retry: loadTimeData.getString('controlRetry'),
            save: loadTimeData.getString('dangerSave'),
            show: loadTimeData.getString('controlShowInFolder'),
          };
        },
      },

      isDangerous_: {
        computed: 'computeIsDangerous_(data_.state)',
        type: Boolean,
        value: false,
      },

      isInProgress_: {
        computed: 'computeIsInProgress_(data_.state)',
        type: Boolean,
        value: false,
      },

      showCancel_: {
        computed: 'computeShowCancel_(data_.state)',
        type: Boolean,
        value: false,
      },

      showProgress_: {
        computed: 'computeShowProgress_(showCancel_, data_.percent)',
        type: Boolean,
        value: false,
      },

      isMalware_: {
        computed: 'computeIsMalware_(isDangerous_, data_.danger_type)',
        type: Boolean,
        value: false,
      },

      // TODO(dbeam): move all properties to |data_|.
      data_: {
        type: Object,
        value: function() { return {}; },
      },
    },

    ready: function() {
      this.content = this.$.content;
      this.resolveReadyPromise_();
    },

    /** @param {!downloads.Data} data */
    update: function(data) {
      assert(!('id' in this.data_) || this.data_.id == data.id);

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
        // TODO(dbeam): remove this once data binding is fully in place.
        return;
      }

      for (var key in data) {
        this.set('data_.' + key, data[key]);
      }

      /** @const */ var isActive =
          data.state != downloads.States.CANCELLED &&
          data.state != downloads.States.INTERRUPTED &&
          !data.file_externally_removed;
      this.$.content.classList.toggle('is-active', isActive);
      this.$.content.elevation = isActive ? 1 : 0;

      // Danger-dependent UI and controls.
      this.$.content.classList.toggle('dangerous', this.isDangerous_);

      var description = this.getDangerText_(data) || this.getStatusText_(data);

      // Status goes in the "tag" (next to the file name) if there's no file.
      this.ensureTextIs_(this.$.description, isActive ? description : '');
      this.ensureTextIs_(this.$.tag, isActive ? '' : description);

      this.$.content.classList.toggle('show-progress', this.showProgress_);

      if (!this.isDangerous_) {
        this.$['file-link'].href = data.url;
        this.ensureTextIs_(this.$['file-link'], data.file_name);

        this.$['file-link'].hidden = !this.completelyOnDisk_;
        this.$.name.hidden = this.completelyOnDisk_;

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
    },

    /** @private */
    computeCompletelyOnDisk_: function() {
      return this.data_.state == downloads.States.COMPLETE &&
             !this.data_.file_externally_removed;
    },

    /** @private */
    computeDate_: function() {
      assert(!this.hideDate);
      return assert(this.data_.since_string || this.data_.date_string);
    },

    /** @private */
    computeIsInProgress_: function() {
      return this.data_.state == downloads.States.IN_PROGRESS;
    },

    /** @private */
    computeIsMalware_: function() {
      return this.isDangerous_ &&
          (this.data_.danger_type == downloads.DangerType.DANGEROUS_CONTENT ||
           this.data_.danger_type == downloads.DangerType.DANGEROUS_HOST ||
           this.data_.danger_type == downloads.DangerType.DANGEROUS_URL ||
           this.data_.danger_type == downloads.DangerType.POTENTIALLY_UNWANTED);
    },

    /** @private */
    computeIsDangerous_: function() {
      return this.data_.state == downloads.States.DANGEROUS;
    },

    /** @private */
    computeRemoveStyle_: function() {
      var canDelete = loadTimeData.getBoolean('allowDeletingHistory');
      var hideRemove = this.isDangerous_ || this.showCancel_ || !canDelete;
      return hideRemove ? 'visibility: hidden' : '';
    },

    /** @private */
    computeShowCancel_: function() {
      return this.data_.state == downloads.States.IN_PROGRESS ||
             this.data_.state == downloads.States.PAUSED;
    },

    /** @private */
    computeShowProgress_: function() {
      return this.showCancel_ && isFinite(this.data_.percent);
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
    isIndeterminate_: function() {
      assert(this.showProgress_);
      return this.data_.percent == -1;
    },

    /** @private */
    onCancelClick_: function() {
      this.actionService_.cancel(this.data_.id);
    },

    /** @private */
    onDiscardDangerous_: function() {
      this.actionService_.discardDangerous(this.data_.id);
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
      this.actionService_.remove(this.data_.id);
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
    onSaveDangerous_: function() {
      this.actionService_.saveDangerous(this.data_.id);
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
