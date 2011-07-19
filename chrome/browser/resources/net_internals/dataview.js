// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays options for importing/exporting the captured data.  This
 * view is responsible for the interface and file I/O.  The BrowserBridge
 * handles creation and loading of the data dumps.
 *
 *   - Has a button to generate a JSON dump.
 *
 *   - Has a button to import a JSON dump.
 *
 *   - Shows how many events have been captured.
 *  @constructor
 */
function DataView(mainBoxId,
                  downloadIframeId,
                  saveFileButtonId,
                  dataViewSaveStatusTextId,
                  securityStrippingCheckboxId,
                  byteLoggingCheckboxId,
                  passivelyCapturedCountId,
                  activelyCapturedCountId,
                  deleteAllId,
                  dumpDataDivId,
                  loadedDivId,
                  loadedClientInfoTextId,
                  loadLogFileDropTargetId,
                  loadLogFileId,
                  dataViewLoadStatusTextId,
                  capturingTextSpanId,
                  loggingTextSpanId) {
  DivView.call(this, mainBoxId);

  var securityStrippingCheckbox =
      document.getElementById(securityStrippingCheckboxId);
  securityStrippingCheckbox.onclick =
      this.onSetSecurityStripping_.bind(this, securityStrippingCheckbox);

  var byteLoggingCheckbox = document.getElementById(byteLoggingCheckboxId);
  byteLoggingCheckbox.onclick =
      this.onSetByteLogging_.bind(this, byteLoggingCheckbox);

  this.downloadIframe_ = document.getElementById(downloadIframeId);

  this.saveFileButton_ = document.getElementById(saveFileButtonId);
  this.saveFileButton_.onclick = this.onSaveFile_.bind(this);
  this.saveStatusText_ = document.getElementById(dataViewSaveStatusTextId);

  this.activelyCapturedCountBox_ =
      document.getElementById(activelyCapturedCountId);
  this.passivelyCapturedCountBox_ =
      document.getElementById(passivelyCapturedCountId);
  document.getElementById(deleteAllId).onclick =
      g_browser.deleteAllEvents.bind(g_browser);

  this.dumpDataDiv_ = document.getElementById(dumpDataDivId);
  this.capturingTextSpan_ = document.getElementById(capturingTextSpanId);
  this.loggingTextSpan_ = document.getElementById(loggingTextSpanId);

  this.loadedDiv_ = document.getElementById(loadedDivId);
  this.loadedClientInfoText_ = document.getElementById(loadedClientInfoTextId);

  this.loadFileElement_ = document.getElementById(loadLogFileId);
  this.loadFileElement_.onchange = this.logFileChanged.bind(this);
  this.loadStatusText_ = document.getElementById(dataViewLoadStatusTextId);

  var dropTarget = document.getElementById(loadLogFileDropTargetId);
  dropTarget.ondragenter = this.onDrag.bind(this);
  dropTarget.ondragover = this.onDrag.bind(this);
  dropTarget.ondrop = this.onDrop.bind(this);

  this.updateEventCounts_();

  g_browser.addLogObserver(this);
}

inherits(DataView, DivView);

/**
 * Called whenever a new event is received.
 */
DataView.prototype.onLogEntryAdded = function(logEntry) {
  this.updateEventCounts_();
};

/**
 * Called whenever some log events are deleted.  |sourceIds| lists
 * the source IDs of all deleted log entries.
 */
DataView.prototype.onLogEntriesDeleted = function(sourceIds) {
  this.updateEventCounts_();
};

/**
 * Called whenever all log events are deleted.
 */
DataView.prototype.onAllLogEntriesDeleted = function() {
  this.updateEventCounts_();
};

/**
 * Called when a log file is loaded, after clearing the old log entries and
 * loading the new ones.  Hides parts of the page related to saving data, and
 * shows those related to loading.  Returns true to indicate the view should
 * still be visible.
 */
DataView.prototype.onLoadLogFinish = function(data) {
  setNodeDisplay(this.dumpDataDiv_, false);
  setNodeDisplay(this.capturingTextSpan_, false);
  setNodeDisplay(this.loggingTextSpan_, true);
  setNodeDisplay(this.loadedDiv_, true);
  this.updateLoadedClientInfo();
  return true;
};

/**
 * Updates the counters showing how many events have been captured.
 */
DataView.prototype.updateEventCounts_ = function() {
  this.activelyCapturedCountBox_.textContent =
      g_browser.getNumActivelyCapturedEvents()
  this.passivelyCapturedCountBox_.textContent =
      g_browser.getNumPassivelyCapturedEvents();
};

/**
 * Depending on the value of the checkbox, enables or disables logging of
 * actual bytes transferred.
 */
DataView.prototype.onSetByteLogging_ = function(byteLoggingCheckbox) {
  if (byteLoggingCheckbox.checked) {
    g_browser.setLogLevel(LogLevelType.LOG_ALL);
  } else {
    g_browser.setLogLevel(LogLevelType.LOG_ALL_BUT_BYTES);
  }
};

/**
 * Depending on the value of the checkbox, enables or disables stripping
 * cookies and passwords from log dumps and displayed events.
 */
DataView.prototype.onSetSecurityStripping_ =
    function(securityStrippingCheckbox) {
  g_browser.setSecurityStripping(securityStrippingCheckbox.checked);
};

DataView.prototype.onSecurityStrippingChanged = function() {
};

/**
 * Called when something is dragged over the drop target.
 *
 * Returns false to cancel default browser behavior when a single file is being
 * dragged.  When this happens, we may not receive a list of files for security
 * reasons, which is why we allow the |files| array to be empty.
 */
DataView.prototype.onDrag = function(event) {
  return event.dataTransfer.types.indexOf('Files') == -1 ||
         event.dataTransfer.files.length > 1;
};

/**
 * Called when something is dropped onto the drop target.  If it's a single
 * file, tries to load it as a log file.
 */
DataView.prototype.onDrop = function(event) {
  if (event.dataTransfer.types.indexOf('Files') == -1 ||
      event.dataTransfer.files.length != 1) {
    return;
  }
  event.preventDefault();

  // Loading a log file may hide the currently active tab.  Switch to the data
  // tab to prevent this.
  document.location.hash = 'data';

  this.loadLogFile(event.dataTransfer.files[0]);
};

/**
 * Called when a log file is selected.
 *
 * Gets the log file from the input element and tries to read from it.
 */
DataView.prototype.logFileChanged = function() {
  this.loadLogFile(this.loadFileElement_.files[0]);
};

/**
 * Attempts to read from the File |logFile|.
 */
DataView.prototype.loadLogFile = function(logFile) {
  if (logFile) {
    this.setLoadFileStatus('Loading log...', true);
    var fileReader = new FileReader();

    fileReader.onload = this.onLoadLogFile.bind(this);
    fileReader.onerror = this.onLoadLogFileError.bind(this);

    fileReader.readAsText(logFile);
  }
};

/**
 * Displays an error message when unable to read the selected log file.
 * Also clears the file input control, so the same file can be reloaded.
 */
DataView.prototype.onLoadLogFileError = function(event) {
  this.loadFileElement_.value = null;
  this.setLoadFileStatus(
      'Error ' +  getKeyWithValue(FileError, event.target.error.code) +
          '.  Unable to read file.',
      false);
};

DataView.prototype.onLoadLogFile = function(event) {
  var result = loadLogFile(event.target.result);
  this.setLoadFileStatus(result, false);
};

/**
 * Sets the load from file status text, displayed below the load file button,
 * to |text|.  Also enables or disables the save/load buttons based on the value
 * of |isLoading|, which must be true if the load process is still ongoing, and
 * false when the operation has stopped, regardless of success of failure.
 * Also, when loading is done, replaces the load button so the same file can be
 * loaded again.
 */
DataView.prototype.setLoadFileStatus = function(text, isLoading) {
  this.enableLoadFileElement_(!isLoading);
  this.enableSaveFileButton_(!isLoading);
  this.loadStatusText_.textContent = text;
  this.saveStatusText_.textContent = '';

  if (!isLoading) {
    // Clear the button, so the same file can be reloaded.  Recreating the
    // element seems to be the only way to do this.
    var loadFileElementId = this.loadFileElement_.id;
    var loadFileElementOnChange = this.loadFileElement_.onchange;
    this.loadFileElement_.outerHTML = this.loadFileElement_.outerHTML;
    this.loadFileElement_ = document.getElementById(loadFileElementId);
    this.loadFileElement_.onchange = loadFileElementOnChange;
  }
};

/**
 * Sets the save to file status text, displayed below the save to file button,
 * to |text|.  Also enables or disables the save/load buttons based on the value
 * of |isSaving|, which must be true if the save process is still ongoing, and
 * false when the operation has stopped, regardless of success of failure.
 */
DataView.prototype.setSaveFileStatus = function(text, isSaving) {
  this.enableSaveFileButton_(!isSaving);
  this.enableLoadFileElement_(!isSaving);
  this.loadStatusText_.textContent = '';
  this.saveStatusText_.textContent = text;
};

DataView.prototype.enableSaveFileButton_ = function(enabled) {
  this.saveFileButton_.disabled = !enabled;
};

DataView.prototype.enableLoadFileElement_ = function(enabled) {
  this.loadFileElement_.disabled = !enabled;
};

/**
 * If not already busy loading or saving a log dump, triggers generation of
 * gathering log dump data and starts waiting for the process to complete.
 */
DataView.prototype.onSaveFile_ = function() {
  if (this.saveFileButton_.disabled)
    return;
  this.setSaveFileStatus('Preparing data...', true);

  createLogDumpAsync(this.onLogDumpCreated.bind(this));
};

/**
 * Starts the process of creating a file containing |dumpText| and downloading
 * it.
 */
DataView.prototype.onLogDumpCreated = function(dumpText) {
  var blobBuilder = new WebKitBlobBuilder();
  blobBuilder.append(dumpText, 'native');
  var textBlob = blobBuilder.getBlob('octet/stream');

  this.setSaveFileStatus('Creating file...', true);

  window.webkitRequestFileSystem(
      window.TEMPORARY, textBlob.size,
      this.onFileSystemCreate_.bind(this, textBlob),
      this.onFileError_.bind(this, 'Unable to create file system.'));
};

/**
 * Once we have access to the file system, create a log file.
 */
DataView.prototype.onFileSystemCreate_ = function(textBlob, fileSystem) {
  fileSystem.root.getFile(
      'net_internals.log', {create: true},
      this.onFileCreate_.bind(this, textBlob),
      this.onFileError_.bind(this, 'Unable to create file.'));
};

/**
 * Once the file is created, or an existing one has been opened, create a
 * writer for it.
 */
DataView.prototype.onFileCreate_ = function(textBlob, fileEntry) {
  fileEntry.createWriter(
      this.onFileCreateWriter_.bind(this, textBlob, fileEntry),
      this.onFileError_.bind(this, 'Unable to create writer.'));
};

/* Once the |fileWriter| has been created, truncate the file, in case it already
 * existed.
 */
DataView.prototype.onFileCreateWriter_ = function(textBlob,
                                                  fileEntry, fileWriter) {
  fileWriter.onerror = this.onFileError_.bind(this, 'Truncate failed.');
  fileWriter.onwriteend = this.onFileTruncate_.bind(this, textBlob,
                                                    fileWriter, fileEntry);
  fileWriter.truncate(0);
};

/**
 * Once the file has been truncated, write |textBlob| to the file.
 */
DataView.prototype.onFileTruncate_ = function(textBlob, fileWriter, fileEntry) {
  fileWriter.onerror = this.onFileError_.bind(this, 'Write failed.');
  fileWriter.onwriteend = this.onFileWriteComplete_.bind(this, fileEntry);
  fileWriter.write(textBlob);
};

/**
 * Once the file has been written to, start the download.
 */
DataView.prototype.onFileWriteComplete_ = function(fileEntry) {
  this.downloadIframe_.src = fileEntry.toURL();
  this.setSaveFileStatus('Dump successful', false);
};

/**
 * On any Javascript File API error, enable the save button and display
 * |errorText|, followed by the specific error.
 */
DataView.prototype.onFileError_ = function(errorText, error) {
  this.enableSaveFileButton_(true);
  this.setSaveFileStatus(
      errorText + '  ' + getKeyWithValue(FileError, error.code),
      false);
};

/**
 * Prints some basic information about the environment when the log was made.
 */
DataView.prototype.updateLoadedClientInfo = function() {
  this.loadedClientInfoText_.textContent = '';
  if (typeof(ClientInfo) != 'object')
    return;

  var text = [];

  // Dumps made with the command line option don't have a date.
  if (ClientInfo.date) {
    text.push('Data exported on: ' + ClientInfo.date);
    text.push('');
  }

  text.push(ClientInfo.name +
            ' ' + ClientInfo.version +
            ' (' + ClientInfo.official +
            ' ' + ClientInfo.cl +
            ') ' + ClientInfo.version_mod);
  text.push('OS Type: ' + ClientInfo.os_type);
  text.push('Command line: ' + ClientInfo.command_line);

  this.loadedClientInfoText_.textContent = text.join('\n');
};
