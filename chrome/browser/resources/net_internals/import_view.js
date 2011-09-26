// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays options for importing data from a log file.
 */
var ImportView = (function() {
  'use strict';

  // This is defined in index.html, but for all intents and purposes is part
  // of this view.
  var LOAD_LOG_FILE_DROP_TARGET_ID = 'import-view-drop-target';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function ImportView() {
    assertFirstConstructorCall(ImportView);

    // Call superclass's constructor.
    superClass.call(this, ImportView.MAIN_BOX_ID);

    this.loadedDiv_ = $(ImportView.LOADED_DIV_ID);

    this.loadFileElement_ = $(ImportView.LOAD_LOG_FILE_ID);
    this.loadFileElement_.onchange = this.logFileChanged.bind(this);
    this.loadStatusText_ = $(ImportView.LOAD_STATUS_TEXT_ID);

    var dropTarget = $(LOAD_LOG_FILE_DROP_TARGET_ID);
    dropTarget.ondragenter = this.onDrag.bind(this);
    dropTarget.ondragover = this.onDrag.bind(this);
    dropTarget.ondrop = this.onDrop.bind(this);

    $(ImportView.RELOAD_LINK_ID).onclick = this.clickedReload_.bind(this);

    this.loadedInfoBuildName_ = $(ImportView.LOADED_INFO_BUILD_NAME_ID);
    this.loadedInfoExportDate_ = $(ImportView.LOADED_INFO_EXPORT_DATE_ID);
    this.loadedInfoOsType_ = $(ImportView.LOADED_INFO_OS_TYPE_ID);
    this.loadedInfoCommandLine_ = $(ImportView.LOADED_INFO_COMMAND_LINE_ID);
    this.loadedInfoUserComments_ = $(ImportView.LOADED_INFO_USER_COMMENTS_ID);
  }

  ImportView.TAB_HANDLE_ID = 'tab-handle-import';

  // IDs for special HTML elements in import_view.html
  ImportView.MAIN_BOX_ID = 'import-view-tab-content';
  ImportView.LOADED_DIV_ID = 'import-view-loaded-div';
  ImportView.LOAD_LOG_FILE_ID = 'import-view-load-log-file';
  ImportView.LOAD_STATUS_TEXT_ID = 'import-view-load-status-text';
  ImportView.RELOAD_LINK_ID = 'import-view-reloaded-link';
  ImportView.LOADED_INFO_EXPORT_DATE_ID = 'import-view-export-date';
  ImportView.LOADED_INFO_BUILD_NAME_ID = 'import-view-build-name';
  ImportView.LOADED_INFO_OS_TYPE_ID = 'import-view-os-type';
  ImportView.LOADED_INFO_COMMAND_LINE_ID = 'import-view-command-line';
  ImportView.LOADED_INFO_USER_COMMENTS_ID = 'import-view-user-comments';

  cr.addSingletonGetter(ImportView);

  ImportView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Called when a log file is loaded, after clearing the old log entries and
     * loading the new ones.  Returns true to indicate the view should
     * still be visible.
     */
    onLoadLogFinish: function(data, unused, userComments) {
      setNodeDisplay(this.loadedDiv_, true);
      this.updateLoadedClientInfo(userComments);
      return true;
    },

    /**
     * Called when the user clicks the "reloaded" link.
     */
    clickedReload_: function() {
      window.location.reload();
      return false;
    },

    /**
     * Called when something is dragged over the drop target.
     *
     * Returns false to cancel default browser behavior when a single file is
     * being dragged.  When this happens, we may not receive a list of files for
     * security reasons, which is why we allow the |files| array to be empty.
     */
    onDrag: function(event) {
      return event.dataTransfer.types.indexOf('Files') == -1 ||
             event.dataTransfer.files.length > 1;
    },

    /**
     * Called when something is dropped onto the drop target.  If it's a single
     * file, tries to load it as a log file.
     */
    onDrop: function(event) {
      if (event.dataTransfer.types.indexOf('Files') == -1 ||
          event.dataTransfer.files.length != 1) {
        return;
      }
      event.preventDefault();

      // Loading a log file may hide the currently active tab.  Switch to the
      // import tab to prevent this.
      document.location.hash = 'import';

      this.loadLogFile(event.dataTransfer.files[0]);
    },

    /**
     * Called when a log file is selected.
     *
     * Gets the log file from the input element and tries to read from it.
     */
    logFileChanged: function() {
      this.loadLogFile(this.loadFileElement_.files[0]);
    },

    /**
     * Attempts to read from the File |logFile|.
     */
    loadLogFile: function(logFile) {
      if (logFile) {
        this.setLoadFileStatus('Loading log...', true);
        var fileReader = new FileReader();

        fileReader.onload = this.onLoadLogFile.bind(this, logFile);
        fileReader.onerror = this.onLoadLogFileError.bind(this);

        fileReader.readAsText(logFile);
      }
    },

    /**
     * Displays an error message when unable to read the selected log file.
     * Also clears the file input control, so the same file can be reloaded.
     */
    onLoadLogFileError: function(event) {
      this.loadFileElement_.value = null;
      this.setLoadFileStatus(
          'Error ' + getKeyWithValue(FileError, event.target.error.code) +
              '.  Unable to read file.',
          false);
    },

    onLoadLogFile: function(logFile, event) {
      var result = logutil.loadLogFile(event.target.result, logFile.fileName);
      this.setLoadFileStatus(result, false);
    },

    /**
     * Sets the load from file status text, displayed below the load file
     * button, to |text|.  Also enables or disables the load buttons based on
     * the value of |isLoading|, which must be true if the load process is still
     * ongoing, and false when the operation has stopped, regardless of success
     * of failure.  Also, when loading is done, replaces the load button so the
     * same file can be loaded again.
     */
    setLoadFileStatus: function(text, isLoading) {
      this.enableLoadFileElement_(!isLoading);
      this.loadStatusText_.textContent = text;

      if (!isLoading) {
        // Clear the button, so the same file can be reloaded.  Recreating the
        // element seems to be the only way to do this.
        var loadFileElementId = this.loadFileElement_.id;
        var loadFileElementOnChange = this.loadFileElement_.onchange;
        this.loadFileElement_.outerHTML = this.loadFileElement_.outerHTML;
        this.loadFileElement_ = $(loadFileElementId);
        this.loadFileElement_.onchange = loadFileElementOnChange;
      }
    },

    enableLoadFileElement_: function(enabled) {
      this.loadFileElement_.disabled = !enabled;
    },

    /**
     * Prints some basic information about the environment when the log was
     * made.
     */
    updateLoadedClientInfo: function(userComments) {
      // Reset all the fields (in case we early-return).
      this.loadedInfoExportDate_.innerText = '';
      this.loadedInfoBuildName_.innerText = '';
      this.loadedInfoOsType_.innerText = '';
      this.loadedInfoCommandLine_.innerText = '';
      this.loadedInfoUserComments_.innerText = '';

      if (typeof(ClientInfo) != 'object')
        return;

      // Dumps made with the command line option don't have a date.
      this.loadedInfoExportDate_.innerText = ClientInfo.date || '';

      var buildName =
          ClientInfo.name +
          ' ' + ClientInfo.version +
          ' (' + ClientInfo.official +
          ' ' + ClientInfo.cl +
          ') ' + ClientInfo.version_mod;

      this.loadedInfoBuildName_.innerText = buildName;

      this.loadedInfoOsType_.innerText = ClientInfo.os_type;
      this.loadedInfoCommandLine_.innerText = ClientInfo.command_line;

      // User comments will not be available when dumped from command line.
      this.loadedInfoUserComments_.innerText = userComments || '';
    }
  };

  return ImportView;
})();
