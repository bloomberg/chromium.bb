// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays options for importing data from a log file.
 *  @constructor
 */
function ImportView() {
  const mainBoxId = 'import-view-tab-content';
  const loadedDivId = 'import-view-loaded-div';
  const loadLogFileDropTargetId = 'import-view-drop-target';
  const loadLogFileId = 'import-view-load-log-file';
  const loadStatusTextId = 'import-view-load-status-text';
  const reloadLinkId = 'import-view-reloaded-link';

  const loadedInfoExportDateId = 'import-view-export-date';
  const loadedInfoBuildNameId = 'import-view-build-name';
  const loadedInfoOsTypeId = 'import-view-os-type';
  const loadedInfoCommandLineId = 'import-view-command-line';
  const loadedInfoUserCommentsId = 'import-view-user-comments';

  DivView.call(this, mainBoxId);

  this.loadedDiv_ = $(loadedDivId);

  this.loadFileElement_ = $(loadLogFileId);
  this.loadFileElement_.onchange = this.logFileChanged.bind(this);
  this.loadStatusText_ = $(loadStatusTextId);

  var dropTarget = $(loadLogFileDropTargetId);
  dropTarget.ondragenter = this.onDrag.bind(this);
  dropTarget.ondragover = this.onDrag.bind(this);
  dropTarget.ondrop = this.onDrop.bind(this);

  $(reloadLinkId).onclick = this.clickedReload_.bind(this);

  this.loadedInfoBuildName_ = $(loadedInfoBuildNameId);
  this.loadedInfoExportDate_ = $(loadedInfoExportDateId);
  this.loadedInfoOsType_ = $(loadedInfoOsTypeId);
  this.loadedInfoCommandLine_ = $(loadedInfoCommandLineId);
  this.loadedInfoUserComments_ = $(loadedInfoUserCommentsId);
}

inherits(ImportView, DivView);

/**
 * Called when a log file is loaded, after clearing the old log entries and
 * loading the new ones.  Returns true to indicate the view should
 * still be visible.
 */
ImportView.prototype.onLoadLogFinish = function(data, unused, userComments) {
  setNodeDisplay(this.loadedDiv_, true);
  this.updateLoadedClientInfo(userComments);
  return true;
};

/**
 * Called when the user clicks the "reloaded" link.
 */
ImportView.prototype.clickedReload_ = function() {
  window.location.reload();
  return false;
};

/**
 * Called when something is dragged over the drop target.
 *
 * Returns false to cancel default browser behavior when a single file is being
 * dragged.  When this happens, we may not receive a list of files for security
 * reasons, which is why we allow the |files| array to be empty.
 */
ImportView.prototype.onDrag = function(event) {
  return event.dataTransfer.types.indexOf('Files') == -1 ||
         event.dataTransfer.files.length > 1;
};

/**
 * Called when something is dropped onto the drop target.  If it's a single
 * file, tries to load it as a log file.
 */
ImportView.prototype.onDrop = function(event) {
  if (event.dataTransfer.types.indexOf('Files') == -1 ||
      event.dataTransfer.files.length != 1) {
    return;
  }
  event.preventDefault();

  // Loading a log file may hide the currently active tab.  Switch to the import
  // tab to prevent this.
  document.location.hash = 'import';

  this.loadLogFile(event.dataTransfer.files[0]);
};

/**
 * Called when a log file is selected.
 *
 * Gets the log file from the input element and tries to read from it.
 */
ImportView.prototype.logFileChanged = function() {
  this.loadLogFile(this.loadFileElement_.files[0]);
};

/**
 * Attempts to read from the File |logFile|.
 */
ImportView.prototype.loadLogFile = function(logFile) {
  if (logFile) {
    this.setLoadFileStatus('Loading log...', true);
    var fileReader = new FileReader();

    fileReader.onload = this.onLoadLogFile.bind(this, logFile);
    fileReader.onerror = this.onLoadLogFileError.bind(this);

    fileReader.readAsText(logFile);
  }
};

/**
 * Displays an error message when unable to read the selected log file.
 * Also clears the file input control, so the same file can be reloaded.
 */
ImportView.prototype.onLoadLogFileError = function(event) {
  this.loadFileElement_.value = null;
  this.setLoadFileStatus(
      'Error ' + getKeyWithValue(FileError, event.target.error.code) +
          '.  Unable to read file.',
      false);
};

ImportView.prototype.onLoadLogFile = function(logFile, event) {
  var result = loadLogFile(event.target.result, logFile.fileName);
  this.setLoadFileStatus(result, false);
};

/**
 * Sets the load from file status text, displayed below the load file button,
 * to |text|.  Also enables or disables the load buttons based on the value
 * of |isLoading|, which must be true if the load process is still ongoing, and
 * false when the operation has stopped, regardless of success of failure.
 * Also, when loading is done, replaces the load button so the same file can be
 * loaded again.
 */
ImportView.prototype.setLoadFileStatus = function(text, isLoading) {
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
};

ImportView.prototype.enableLoadFileElement_ = function(enabled) {
  this.loadFileElement_.disabled = !enabled;
};

/**
 * Prints some basic information about the environment when the log was made.
 */
ImportView.prototype.updateLoadedClientInfo = function(userComments) {
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
};
