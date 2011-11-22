// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// Helper functions
function $(o) {return document.getElementById(o);}

/**
 * Sets the display style of a node.
 */
function showInline(node, isShow) {
  node.style.display = isShow ? 'inline' : 'none';
}

function showInlineBlock(node, isShow) {
  node.style.display = isShow ? 'inline-block' : 'none';
}

/**
 * Creates an element of a specified type with a specified class name.
 * @param {String} type The node type.
 * @param {String} className The class name to use.
 */
function createElementWithClassName(type, className) {
  var elm = document.createElement(type);
  elm.className = className;
  return elm;
}

/**
 * Creates a link with a specified onclick handler and content
 * @param {String} onclick The onclick handler
 * @param {String} value The link text
 */
function createLink(onclick, value) {
  var link = document.createElement('a');
  link.onclick = onclick;
  link.href = '#';
  link.textContent = value;
  link.oncontextmenu = function() { return false; };
  return link;
}

/**
 * Creates a button with a specified onclick handler and content
 * @param {String} onclick The onclick handler
 * @param {String} value The button text
 */
function createButton(onclick, value) {
  var button = document.createElement('input');
  button.type = 'button';
  button.value = value;
  button.onclick = onclick;
  return button;
}

///////////////////////////////////////////////////////////////////////////////
// Downloads
/**
 * Class to hold all the information about the visible downloads.
 */
function Downloads() {
  this.downloads_ = {};
  this.node_ = $('downloads-display');
  this.summary_ = $('downloads-summary-text');
  this.searchText_ = '';

  // Keep track of the dates of the newest and oldest downloads so that we
  // know where to insert them.
  this.newestTime_ = -1;
}

/**
 * Called when a download has been updated or added.
 * @param {Object} download A backend download object (see downloads_ui.cc)
 */
Downloads.prototype.updated = function(download) {
  var id = download.id;
  if (!!this.downloads_[id]) {
    this.downloads_[id].update(download);
  } else {
    this.downloads_[id] = new Download(download);
    // We get downloads in display order, so we don't have to worry about
    // maintaining correct order - we can assume that any downloads not in
    // display order are new ones and so we can add them to the top of the
    // list.
    if (download.started > this.newestTime_) {
      this.node_.insertBefore(this.downloads_[id].node, this.node_.firstChild);
      this.newestTime_ = download.started;
    } else {
      this.node_.appendChild(this.downloads_[id].node);
    }
    this.updateDateDisplay_();
  }
}

/**
 * Set our display search text.
 * @param {String} searchText The string we're searching for.
 */
Downloads.prototype.setSearchText = function(searchText) {
  this.searchText_ = searchText;
}

/**
 * Update the summary block above the results
 */
Downloads.prototype.updateSummary = function() {
  if (this.searchText_) {
    this.summary_.textContent = localStrings.getStringF('searchresultsfor',
                                                        this.searchText_);
  } else {
    this.summary_.textContent = localStrings.getString('downloads');
  }

  var hasDownloads = false;
  for (var i in this.downloads_) {
    hasDownloads = true;
    break;
  }

  if (!hasDownloads) {
    this.node_.textContent = localStrings.getString('noresults');
  }
}

/**
 * Update the date visibility in our nodes so that no date is
 * repeated.
 */
Downloads.prototype.updateDateDisplay_ = function() {
  var dateContainers = document.getElementsByClassName('date-container');
  var displayed = {};
  for (var i = 0, container; container = dateContainers[i]; i++) {
    var dateString = container.getElementsByClassName('date')[0].innerHTML;
    if (!!displayed[dateString]) {
      container.style.display = 'none';
    } else {
      displayed[dateString] = true;
      container.style.display = 'block';
    }
  }
}

/**
 * Remove a download.
 * @param {Number} id The id of the download to remove.
 */
Downloads.prototype.remove = function(id) {
  this.node_.removeChild(this.downloads_[id].node);
  delete this.downloads_[id];
  this.updateDateDisplay_();
}

/**
 * Clear all downloads and reset us back to a null state.
 */
Downloads.prototype.clear = function() {
  for (var id in this.downloads_) {
    this.downloads_[id].clear();
    this.remove(id);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Download
/**
 * A download and the DOM representation for that download.
 * @param {Object} download A backend download object (see downloads_ui.cc)
 */
function Download(download) {
  // Create DOM
  this.node = createElementWithClassName('div','download' +
                                         (download.otr ? ' otr' : ''));

  // Dates
  this.dateContainer_ = createElementWithClassName('div', 'date-container');
  this.node.appendChild(this.dateContainer_);

  this.nodeSince_ = createElementWithClassName('div', 'since');
  this.nodeDate_ = createElementWithClassName('div', 'date');
  this.dateContainer_.appendChild(this.nodeSince_);
  this.dateContainer_.appendChild(this.nodeDate_);

  // Container for all 'safe download' UI.
  this.safe_ = createElementWithClassName('div', 'safe');
  this.safe_.ondragstart = this.drag_.bind(this);
  this.node.appendChild(this.safe_);

  if (download.state != Download.States.COMPLETE) {
    this.nodeProgressBackground_ =
        createElementWithClassName('div', 'progress background');
    this.safe_.appendChild(this.nodeProgressBackground_);

    this.canvasProgress_ =
        document.getCSSCanvasContext('2d', 'canvas_' + download.id,
            Download.Progress.width,
            Download.Progress.height);

    this.nodeProgressForeground_ =
        createElementWithClassName('div', 'progress foreground');
    this.nodeProgressForeground_.style.webkitMask =
        '-webkit-canvas(canvas_'+download.id+')';
    this.safe_.appendChild(this.nodeProgressForeground_);
  }

  this.nodeImg_ = createElementWithClassName('img', 'icon');
  this.safe_.appendChild(this.nodeImg_);

  // FileLink is used for completed downloads, otherwise we show FileName.
  this.nodeTitleArea_ = createElementWithClassName('div', 'title-area');
  this.safe_.appendChild(this.nodeTitleArea_);

  this.nodeFileLink_ = createLink(this.openFile_.bind(this), '');
  this.nodeFileLink_.className = 'name';
  this.nodeFileLink_.style.display = 'none';
  this.nodeTitleArea_.appendChild(this.nodeFileLink_);

  this.nodeFileName_ = createElementWithClassName('span', 'name');
  this.nodeFileName_.style.display = 'none';
  this.nodeTitleArea_.appendChild(this.nodeFileName_);

  this.nodeStatus_ = createElementWithClassName('span', 'status');
  this.nodeTitleArea_.appendChild(this.nodeStatus_);

  var nodeURLDiv = createElementWithClassName('div', 'url-container');
  this.safe_.appendChild(nodeURLDiv);

  this.nodeURL_ = createElementWithClassName('a', 'src-url');
  this.nodeURL_.target = "_blank";
  nodeURLDiv.appendChild(this.nodeURL_);

  // Controls.
  this.nodeControls_ = createElementWithClassName('div', 'controls');
  this.safe_.appendChild(this.nodeControls_);

  // We don't need "show in folder" in chromium os. See download_ui.cc and
  // http://code.google.com/p/chromium-os/issues/detail?id=916.
  var showinfolder = localStrings.getString('control_showinfolder');
  if (showinfolder) {
    this.controlShow_ = createLink(this.show_.bind(this), showinfolder);
    this.nodeControls_.appendChild(this.controlShow_);
  } else {
    this.controlShow_ = null;
  }

  this.controlRetry_ = document.createElement('a');
  this.controlRetry_.textContent = localStrings.getString('control_retry');
  this.nodeControls_.appendChild(this.controlRetry_);

  // Pause/Resume are a toggle.
  this.controlPause_ = createLink(this.togglePause_.bind(this),
      localStrings.getString('control_pause'));
  this.nodeControls_.appendChild(this.controlPause_);

  this.controlResume_ = createLink(this.togglePause_.bind(this),
      localStrings.getString('control_resume'));
  this.nodeControls_.appendChild(this.controlResume_);

  this.controlRemove_ = createLink(this.remove_.bind(this),
      localStrings.getString('control_removefromlist'));
  this.nodeControls_.appendChild(this.controlRemove_);

  this.controlCancel_ = createLink(this.cancel_.bind(this),
      localStrings.getString('control_cancel'));
  this.nodeControls_.appendChild(this.controlCancel_);

  // Container for 'unsafe download' UI.
  this.danger_ = createElementWithClassName('div', 'show-dangerous');
  this.node.appendChild(this.danger_);

  this.dangerDesc_ = document.createElement('div');
  this.danger_.appendChild(this.dangerDesc_);

  this.dangerSave_ = createButton(this.saveDangerous_.bind(this),
      localStrings.getString('danger_save'));
  this.danger_.appendChild(this.dangerSave_);

  this.dangerDiscard_ = createButton(this.discardDangerous_.bind(this),
      localStrings.getString('danger_discard'));
  this.danger_.appendChild(this.dangerDiscard_);

  // Update member vars.
  this.update(download);
}

/**
 * The states a download can be in. These correspond to states defined in
 * DownloadsDOMHandler::CreateDownloadItemValue
 */
Download.States = {
  IN_PROGRESS : "IN_PROGRESS",
  CANCELLED : "CANCELLED",
  COMPLETE : "COMPLETE",
  PAUSED : "PAUSED",
  DANGEROUS : "DANGEROUS",
  INTERRUPTED : "INTERRUPTED",
}

/**
 * Explains why a download is in DANGEROUS state.
 */
Download.DangerType = {
  NOT_DANGEROUS: "NOT_DANGEROUS",
  DANGEROUS_FILE: "DANGEROUS_FILE",
  DANGEROUS_URL: "DANGEROUS_URL",
  DANGEROUS_CONTENT: "DANGEROUS_CONTENT"
}

/**
 * Constants for the progress meter.
 */
Download.Progress = {
  width : 48,
  height : 48,
  radius : 24,
  centerX : 24,
  centerY : 24,
  base : -0.5 * Math.PI,
  dir : false,
}

/**
 * Updates the download to reflect new data.
 * @param {Object} download A backend download object (see downloads_ui.cc)
 */
Download.prototype.update = function(download) {
  this.id_ = download.id;
  this.filePath_ = download.file_path;
  this.fileUrl_ = download.file_url;
  this.fileName_ = download.file_name;
  this.url_ = download.url;
  this.state_ = download.state;
  this.fileExternallyRemoved_ = download.file_externally_removed;
  this.dangerType_ = download.danger_type;

  this.since_ = download.since_string;
  this.date_ = download.date_string;

  // See DownloadItem::PercentComplete
  this.percent_ = Math.max(download.percent, 0);
  this.progressStatusText_ = download.progress_status_text;
  this.received_ = download.received;

  if (this.state_ == Download.States.DANGEROUS) {
    if (this.dangerType_ == Download.DangerType.DANGEROUS_FILE) {
      this.dangerDesc_.textContent = localStrings.getStringF('danger_file_desc',
                                                             this.fileName_);
    } else {
      // This is used by both DANGEROUS_URL and DANGEROUS_CONTENT.
      this.dangerDesc_.textContent = localStrings.getString('danger_url_desc');
    }
    this.danger_.style.display = 'block';
    this.safe_.style.display = 'none';
  } else {
    this.nodeImg_.src = 'chrome://fileicon/' + this.filePath_;

    if (this.state_ == Download.States.COMPLETE &&
        !this.fileExternallyRemoved_) {
      this.nodeFileLink_.textContent = this.fileName_;
      this.nodeFileLink_.href = this.fileUrl_;
      this.nodeFileLink_.oncontextmenu = null;
    } else if (this.nodeFileName_.textContent != this.fileName_) {
      this.nodeFileName_.textContent = this.fileName_;
    }

    showInline(this.nodeFileLink_,
               this.state_ == Download.States.COMPLETE &&
                   !this.fileExternallyRemoved_);
    // nodeFileName_ has to be inline-block to avoid the 'interaction' with
    // nodeStatus_. If both are inline, it appears that their text contents
    // are merged before the bidi algorithm is applied leading to an
    // undesirable reordering. http://crbug.com/13216
    showInlineBlock(this.nodeFileName_,
                    this.state_ != Download.States.COMPLETE ||
                        this.fileExternallyRemoved_);

    if (this.state_ == Download.States.IN_PROGRESS) {
      this.nodeProgressForeground_.style.display = 'block';
      this.nodeProgressBackground_.style.display = 'block';

      // Draw a pie-slice for the progress.
      this.canvasProgress_.clearRect(0, 0,
                                     Download.Progress.width,
                                     Download.Progress.height);
      this.canvasProgress_.beginPath();
      this.canvasProgress_.moveTo(Download.Progress.centerX,
                                  Download.Progress.centerY);

      // Draw an arc CW for both RTL and LTR. http://crbug.com/13215
      this.canvasProgress_.arc(Download.Progress.centerX,
                               Download.Progress.centerY,
                               Download.Progress.radius,
                               Download.Progress.base,
                               Download.Progress.base + Math.PI * 0.02 *
                               Number(this.percent_),
                               false);

      this.canvasProgress_.lineTo(Download.Progress.centerX,
                                  Download.Progress.centerY);
      this.canvasProgress_.fill();
      this.canvasProgress_.closePath();
    } else if (this.nodeProgressBackground_) {
      this.nodeProgressForeground_.style.display = 'none';
      this.nodeProgressBackground_.style.display = 'none';
    }

    if (this.controlShow_) {
      showInline(this.controlShow_,
                 this.state_ == Download.States.COMPLETE &&
                     !this.fileExternallyRemoved_);
    }
    showInline(this.controlRetry_, this.state_ == Download.States.CANCELLED);
    this.controlRetry_.href = this.url_;
    showInline(this.controlPause_, this.state_ == Download.States.IN_PROGRESS);
    showInline(this.controlResume_, this.state_ == Download.States.PAUSED);
    var showCancel = this.state_ == Download.States.IN_PROGRESS ||
                     this.state_ == Download.States.PAUSED;
    showInline(this.controlCancel_, showCancel);
    showInline(this.controlRemove_, !showCancel);

    this.nodeSince_.textContent = this.since_;
    this.nodeDate_.textContent = this.date_;
    // Don't unnecessarily update the url, as doing so will remove any
    // text selection the user has started (http://crbug.com/44982).
    if (this.nodeURL_.textContent != this.url_) {
      this.nodeURL_.textContent = this.url_;
      this.nodeURL_.href = this.url_;
    }
    this.nodeStatus_.textContent = this.getStatusText_();

    this.danger_.style.display = 'none';
    this.safe_.style.display = 'block';
  }
}

/**
 * Removes applicable bits from the DOM in preparation for deletion.
 */
Download.prototype.clear = function() {
  this.safe_.ondragstart = null;
  this.nodeFileLink_.onclick = null;
  if (this.controlShow_) {
    this.controlShow_.onclick = null;
  }
  this.controlCancel_.onclick = null;
  this.controlPause_.onclick = null;
  this.controlResume_.onclick = null;
  this.dangerDiscard_.onclick = null;

  this.node.innerHTML = '';
}

/**
 * @return {String} User-visible status update text.
 */
Download.prototype.getStatusText_ = function() {
  switch (this.state_) {
    case Download.States.IN_PROGRESS:
      return this.progressStatusText_;
    case Download.States.CANCELLED:
      return localStrings.getString('status_cancelled');
    case Download.States.PAUSED:
      return localStrings.getString('status_paused');
    case Download.States.DANGEROUS:
      // danger_url_desc is also used by DANGEROUS_CONTENT.
      var desc = this.dangerType_ == Download.DangerType.DANGEROUS_FILE ?
          'danger_file_desc' : 'danger_url_desc';
      return localStrings.getString(desc);
    case Download.States.INTERRUPTED:
      return localStrings.getString('status_interrupted');
    case Download.States.COMPLETE:
      return this.fileExternallyRemoved_ ?
          localStrings.getString('status_removed') : '';
  }
}

/**
 * Tells the backend to initiate a drag, allowing users to drag
 * files from the download page and have them appear as native file
 * drags.
 */
Download.prototype.drag_ = function() {
  chrome.send('drag', [this.id_.toString()]);
  return false;
}

/**
 * Tells the backend to open this file.
 */
Download.prototype.openFile_ = function() {
  chrome.send('openFile', [this.id_.toString()]);
  return false;
}

/**
 * Tells the backend that the user chose to save a dangerous file.
 */
Download.prototype.saveDangerous_ = function() {
  chrome.send('saveDangerous', [this.id_.toString()]);
  return false;
}

/**
 * Tells the backend that the user chose to discard a dangerous file.
 */
Download.prototype.discardDangerous_ = function() {
  chrome.send('discardDangerous', [this.id_.toString()]);
  downloads.remove(this.id_);
  return false;
}

/**
 * Tells the backend to show the file in explorer.
 */
Download.prototype.show_ = function() {
  chrome.send('show', [this.id_.toString()]);
  return false;
}

/**
 * Tells the backend to pause this download.
 */
Download.prototype.togglePause_ = function() {
  chrome.send('togglepause', [this.id_.toString()]);
  return false;
}

/**
 * Tells the backend to remove this download from history and download shelf.
 */
 Download.prototype.remove_ = function() {
  chrome.send('remove', [this.id_.toString()]);
  return false;
}

/**
 * Tells the backend to cancel this download.
 */
Download.prototype.cancel_ = function() {
  chrome.send('cancel', [this.id_.toString()]);
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// Page:
var downloads, localStrings, resultsTimeout;

/**
 * The FIFO array that stores updates of download files to be appeared
 * on the download page. It is guaranteed that the updates in this array
 * are reflected to the download page in a FIFO order.
*/
var fifo_results;

function load() {
  fifo_results = new Array();
  localStrings = new LocalStrings();
  downloads = new Downloads();
  $('term').focus();
  $('term').setAttribute('aria-labelledby', 'search-submit');
  setSearch('');
}

function setSearch(searchText) {
  fifo_results.length = 0;
  downloads.clear();
  downloads.setSearchText(searchText);
  chrome.send('getDownloads', [searchText.toString()]);
}

function clearAll() {
  fifo_results.length = 0;
  downloads.clear();
  downloads.setSearchText('');
  chrome.send('clearAll', []);
  return false;
}

function openDownloadsFolder() {
  chrome.send('openDownloadsFolder');
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// Chrome callbacks:
/**
 * Our history system calls this function with results from searches or when
 * downloads are added or removed.
 */
function downloadsList(results) {
  if (resultsTimeout)
    clearTimeout(resultsTimeout);
  fifo_results.length = 0;
  downloads.clear();
  downloadUpdated(results);
  downloads.updateSummary();
}

/**
 * When a download is updated (progress, state change), this is called.
 */
function downloadUpdated(results) {
  // Sometimes this can get called too early.
  if (!downloads)
    return;

  fifo_results = fifo_results.concat(results);
  tryDownloadUpdatedPeriodically();
}

/**
 * Try to reflect as much updates as possible within 50ms.
 * This function is scheduled again and again until all updates are reflected.
 */
function tryDownloadUpdatedPeriodically() {
  var start = Date.now();
  while (fifo_results.length) {
    var result = fifo_results.shift();
    downloads.updated(result);
    // Do as much as we can in 50ms.
    if (Date.now() - start > 50) {
      clearTimeout(resultsTimeout);
      resultsTimeout = setTimeout(tryDownloadUpdatedPeriodically, 5);
      break;
    }
  }
}

// Add handlers to HTML elements.
document.body.onload = load;

var clearAllLink = $('clear-all');
clearAllLink.onclick = function () { clearAll(''); };
clearAllLink.oncontextmenu = function() { return false; };

var openDownloadsFolderLink = $('open-downloads-folder');
openDownloadsFolderLink.onclick = openDownloadsFolder;
openDownloadsFolderLink.oncontextmenu = function() { return false; };

$('search-link').onclick = function () {
  setSearch('');
  return false;
};
$('search-form').onsubmit = function () {
  setSearch(this.term.value);
  return false;
};
