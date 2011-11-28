// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Wrapper for chrome.send.
 */
function chromeSend(func, arg) {
  if (arg == undefined)
    arg = '';

  // Convert to string.
  if (typeof arg == 'number')
    arg = '' + arg;

  chrome.send(func, [arg]);
};

/**
 * Create a child element.
 *
 * @param {string} type The type - div, span, etc.
 * @param {string} className The class name
 * @param {HTMLElement} parent Parent to append this child to.
 * @param {string} textContent optional text content of child.
 * @param {function(*)} onclick onclick function of child.
 */
function createChild(type, className, parent, textContent, onclick) {
  var elem = document.createElement(type);
  elem.className = className;
  if (textContent !== undefined)
    elem.textContent = textContent;
  elem.onclick = onclick;
  parent.appendChild(elem);
  return elem;
};

var localStrings;
var downloadRowList;

function init() {
  localStrings = new LocalStrings();
  initTestHarness();

  window.onkeydown = function(e) {
    if (e.keyCode == 27)  // Escape.
      menu.clear();
    e.preventDefault();  // Suppress browser shortcuts.
  };

  document.body.addEventListener("blur", menu.clear);
  document.body.addEventListener("click", menu.clear);
  document.body.addEventListener("contextmenu", function (e) {
      e.preventDefault(); });
  document.body.addEventListener("selectstart", function (e) {
      e.preventDefault(); });

  var sadt = $('showallfilestext');
  sadt.textContent = localStrings.getString('showallfiles');
  sadt.addEventListener("click", showAllFiles);

  downloadRowList = new DownloadRowList();
  chromeSend('getDownloads');
}

/**
 * Testing. Allow this page to be loaded in a browser.
 * Create stubs for localStrings and chrome.send.
 */
function initTestHarness() {
  if (location.protocol != 'file:')
    return;

  // Enable right click for dom inspector.
  document.body.oncontextmenu = '';

  // Fix localStrings.
  localStrings = {
    getString: function(name) {
      if (name == 'showallfiles')
        return 'Show all files';
      if (name == 'dangerousextension')
        return 'Extensions, apps, and themes can harm your computer.' +
            ' Are you sure you want to continue?'
      if (name == 'continue')
        return 'Continue';
      if (name == 'discard')
        return 'Discard';
      return name;
    },
    getStringF: function(name, path) {
      return path + ' - Unknown file type.';
    },
  };

  // Log chrome.send calls.
  chrome.send = function(name, ary) {
    console.log('chrome.send ' + name + ' ' + ary);
    if (name == 'getDownloads' ||
        (name == 'openNewFullWindow' &&
        ary[0] == 'chrome://downloads'))
      sendTestResults();
  };

  // Fix resource images.
  var cssRules = document.styleSheets[0].cssRules;
  for (var i = 0; i < cssRules.length; i++) {
    var cssRule = cssRules[i];
    if (cssRule.selectorText.match(/^div\.icon|^\.menuicon/)) {
      cssRule.style.backgroundImage =
          cssRule.style.backgroundImage.replace('chrome://resources', 'shared');
    }
  }
}

/**
 * Create a results array with test data and call downloadsList.
 */
var testElement;
var testId = 0;
var testResults = [];
function sendTestResults() {
  var testState = (testId % 3 == 0 ? 'IN_PROGRESS' :
      (testId % 3 == 1 ? 'DANGEROUS' : 'COMPLETE'));
  state1 = (testId % 3 == 0);
  testResults.push({
      state: testState,
      percent: (testId % 3 == 0 ? 90 : 100),
      id: testId,
      file_name: ' Test' + testId + '.pdf',
      file_path: '/home/achuith/Downloads/Test' + testId + '.pdf',
      progress_status_text : '107 MB/s - 108 MB of 672 MB, 5 secs left',
    });
  testId++;
  downloadsList(testResults);
}

/**
 * Current Menu.
 */
var menu = {
  current_: null,

  /**
   * Close the current menu.
   */
  clear: function() {
    var current = this.current_;
    if (current) {
      current.firstChild.style.display = 'none';
      current.style.opacity = '';
      this.current_ = null;
    }
  },

  /**
   * If it's a second click on an open menu, close the menu.
   * Otherwise, close any other open menu and open the clicked menu.
   */
  clicked: function(row) {
    var menuicon = row.menuicon;
    if (this.current_ === menuicon) {
      this.clear();
      return;
    }
    this.clear();
    if (menuicon.firstChild.style.display != 'block') {
      menuicon.firstChild.style.display = 'block';
      menuicon.style.opacity = '1';
      menuicon.scrollIntoView();
      this.current_ = menuicon;
    }
    window.event.stopPropagation();
  },
};

function DiscardResult(result) {
    return (result.state == 'CANCELLED' ||
            result.state == 'INTERRUPTED' ||
            result.state == 'REMOVING');
};

/**
 * C++ api calls.
 */
function downloadsList(results) {
  downloadRowList.list(results);
}

function downloadUpdated(result) {
  downloadRowList.update(result);
}

function showAllFiles() {
  chromeSend('showAllFiles');
}

/**
 * DownloadRow contains all the elements that go into a row of the downloads
 * list. It represents a single DownloadItem.
 *
 * @param {DownloadRowList} list Global DownloadRowList.
 * @param {Object} result JSON representation of DownloadItem.
 * @constructor
 */
function DownloadRow(list, result) {
  this.path = result.file_path;
  this.name = result.file_name;
  this.fileUrl = result.file_url;
  this.list = list;
  this.id = result.id;

  this.createRow_(list);
  this.createMenu_();
  this.createRowButton_();
  this.setMenuHidden_(true);
}

DownloadRow.prototype = {
  /**
   * Create the row html element and book-keeping for the row.
   * @param {DownloadRowList} list global DownloadRowList instance.
   * @private
   */
  createRow_: function(list) {
    var elem = document.createElement('li');
    elem.className = 'downloadrow';
    elem.id = this.path;
    elem.row = this;
    this.element = elem;

    list.append(this);
  },

  setErrorText_: function(text) {
    this.filename.textContent = text;
  },

  supportsPdf_: function() {
    return 'application/pdf' in navigator.mimeTypes;
  },

  openFilePath_: function() {
    chromeSend('openNewFullWindow', this.fileUrl);
  },

  /**
   * Determine onclick behavior based on filename.
   * @private
   */
  getFunctionForItem_: function() {
    var path = this.path;
    var self = this;

    if (pathIsAudioFile(path)) {
      return function() {
        chromeSend('playMediaFile', path);
      };
    }
    if (pathIsVideoFile(path)) {
      return function() {
        chromeSend('playMediaFile', path);
      };
    }
    if (pathIsImageFile(path)) {
      return function() {
        self.openFilePath_();
      }
    }
    if (pathIsHtmlFile(path)) {
      return function() {
        self.openFilePath_();
      }
    }
    if (pathIsPdfFile(path) && this.supportsPdf_()) {
      return function() {
        self.openFilePath_();
      }
    }

    return function() {
      self.setErrorText_(localStrings.getStringF('error_unknown_file_type',
                          self.name));
    };
  },

  setDangerousIcon_: function(warning) {
    this.icon.className = warning ? 'iconwarning' : 'icon';
    this.icon.style.background = warning ? '' :
        'url(chrome://fileicon' + escape(this.path) +
        '?iconsize=small) no-repeat';
  },

  /**
   * Create the row button for the left of the row.
   * This contains the icon, filename and error elements.
   * @private
   */
  createRowButton_: function () {
    this.rowbutton = createChild('div', 'rowbutton rowbg', this.element);

    // Icon.
    this.icon = createChild('div', 'icon', this.rowbutton);
    this.setDangerousIcon_(false);

    // Filename.
    this.filename = createChild('span', 'title', this.rowbutton, this.name);
  },

  setMenuHidden_: function(hidden) {
    this.menubutton.hidden = hidden;
    if (hidden) {
      this.rowbutton.style.width = '238px';
    } else {
      this.rowbutton.style.width = '';
    }
  },

  /**
   * Create the menu button on the right of the row.
   * This contains the menuicon. The menuicon contains the menu, which
   * contains items for Pause/Resume and Cancel.
   * @private
   */
  createMenu_: function() {
    var self = this;
    this.menubutton = createChild('div', 'menubutton rowbg', this.element, '',
                                  function() {
                                    menu.clicked(self);
                                  });

    this.menuicon = createChild('div', 'menuicon', this.menubutton);

    var menudiv = createChild('div', 'menu', this.menuicon);

    this.pause = createChild('div', 'menuitem', menudiv,
      localStrings.getString('pause'), function() {
                                         self.pauseToggleDownload_();
                                       });

    this.cancel = createChild('div', 'menuitem', menudiv,
      localStrings.getString('cancel'), function() {
                                          self.cancelDownload_();
                                        });
  },

  allowDownload_: function() {
    chromeSend('allowDownload', this.id);
  },

  cancelDownload_: function() {
    chromeSend('cancelDownload', this.id);
  },

  pauseToggleDownload_: function() {
    this.pause.textContent =
      (this.pause.textContent == localStrings.getString('pause')) ?
      localStrings.getString('resume') :
      localStrings.getString('pause');

    chromeSend('pauseToggleDownload', this.id);
  },

  changeElemHeight_: function(elem, x) {
    elem.style.height = elem.clientHeight + x + 'px';
  },

  changeRowHeight_: function(x) {
    this.list.rowsHeight += x;
    this.changeElemHeight_(this.element, x);
    // rowbutton has 5px padding.
    this.changeElemHeight_(this.rowbutton, x - 5);
    this.list.resize();
  },

  DANGEROUS_HEIGHT: 60,
  createDangerousPrompt_: function(dangerType) {
    if (this.dangerous)
      return;

    this.dangerous = createChild('div', 'dangerousprompt', this.rowbutton);

    // Handle dangerous files, extensions and dangerous urls.
    var dangerText;
    if (dangerType == 'DANGEROUS_URL') {
      dangerText = localStrings.getString('dangerousurl');
    } else if (dangerType == 'DANGEROUS_CONTENT') {
      dangerText = localStrings.getStringF('dangerouscontent', this.name);
    } else if (dangerType == 'DANGEROUS_FILE' && this.path.match(/\.crx$/)) {
      dangerText = localStrings.getString('dangerousextension');
    } else {
      dangerText = localStrings.getStringF('dangerousfile', this.name);
    }
    createChild('span', 'dangerousprompttext', this.dangerous, dangerText);

    var self = this;
    createChild('span', 'confirm', this.dangerous,
        localStrings.getString('discard'),
        function() {
          self.cancelDownload_();
        });
    createChild('span', 'confirm', this.dangerous,
        localStrings.getString('continue'),
        function() {
          self.allowDownload_();
        });

    this.changeRowHeight_(this.DANGEROUS_HEIGHT);
    this.setDangerousIcon_(true);
  },

  removeDangerousPrompt_: function() {
    if (!this.dangerous)
      return;

    this.rowbutton.removeChild(this.dangerous);
    this.dangerous = null;

    this.changeRowHeight_(-this.DANGEROUS_HEIGHT);
    this.setDangerousIcon_(false);
  },

  PROGRESS_HEIGHT: 8,
  createProgress_: function() {
    if (this.progress)
      return;

    this.progress = createChild('div', 'progress', this.rowbutton);

    this.setMenuHidden_(false);
    this.changeRowHeight_(this.PROGRESS_HEIGHT);
  },

  removeProgress_: function() {
    if (!this.progress)
      return;

    this.rowbutton.removeChild(this.progress);
    this.progress = null;

    this.changeRowHeight_(-this.PROGRESS_HEIGHT);
    this.setMenuHidden_(true);
  },

  updatePause_: function(result) {
    var pause = this.pause;
    var pauseStr = localStrings.getString('pause');
    var resumeStr = localStrings.getString('resume');

    if (pause &&
        result.state == 'PAUSED' &&
        pause.textContent != resumeStr) {
      pause.textContent = resumeStr;
    } else if (pause &&
              result.state == 'IN_PROGRESS' &&
              pause.textContent != pauseStr) {
      pause.textContent = pauseStr;
    }
  },

  progressStatusText_: function(progress) {
    if (!progress)
      return progress;

    /* m looks like this:
    ["107 MB/s - 108 MB of 672 MB, 5 secs left",
    "107 MB/s", "108", "MB", "672", "MB", "5 secs left"]
    We want to return progress text like this:
    "108 / 672 MB, 5 secs left"
    or
    "108 kB / 672 MB, 5 secs left"
    */
    var m = progress.match(
      /([^-]*) - ([0-9\.]*) ([a-zA-Z]*) of ([0-9\.]*) ([a-zA-Z]*), (.*)/);
    if (!m || m.length != 7)
      return progress;

    return m[2] + (m[3] == m[5] ? '' : ' ' + m[3]) +
        ' / ' + m[4] + ' ' + m[5] + ', ' + m[6];
  },

  updateProgress_: function(result) {
    this.removeDangerousPrompt_();
    this.createProgress_();
    this.progress.textContent =
        this.progressStatusText_(result.progress_status_text);
    this.updatePause_(result);
  },

  /**
   * Called when the item has finished downloading. Switch the menu
   * and remove the progress bar.
   * @private
   */
  finishedDownloading_: function() {
    // Make rowbutton clickable.
    this.rowbutton.onclick = this.getFunctionForItem_();
    this.rowbutton.style.cursor = 'pointer';

    // Make rowbutton draggable.
    this.rowbutton.setAttribute('draggable', 'true');
    var self = this;
    this.rowbutton.addEventListener('dragstart', function(e) {
      e.dataTransfer.effectAllowed = 'copy';
      e.dataTransfer.setData('Text', self.path);
      e.dataTransfer.setData('URL', self.fileUrl);
    }, false);

    this.removeDangerousPrompt_();
    this.removeProgress_();
  },

  /**
   * One of the DownloadItem we are observing has updated.
   * @param {Object} result JSON representation of DownloadItem.
   */
  update: function(result) {
    this.filename.textContent = result.file_name;
    this.id = result.id;

    if (result.state != 'COMPLETE') {
      this.rowbutton.onclick = '';
      this.rowbutton.style.cursor = '';
    }

    if (DiscardResult(result)) {
      this.list.remove(this);
    } else if (result.state == 'DANGEROUS') {
      this.createDangerousPrompt_(result.danger_type);
    } else if (result.percent < 100) {
      this.updateProgress_(result);
    } else if (result.state == 'COMPLETE') {
      this.finishedDownloading_();
    }
  },
};

/**
 * DownloadRowList is a container for DownloadRows.
 */
function DownloadRowList() {
  this.element = createChild('ul', 'downloadlist', $('main'));

  document.title = localStrings.getString('downloadpath').
      split('/').pop();
}

DownloadRowList.prototype = {

  /**
   * numRows is the current number of rows.
   * rowsHeight is the sum of the heights of all rows.
   * rowListHeight is the height of the container containing the rows.
   * rows is the list of DownloadRows.
   */
  numRows: 0,
  rowsHeight: 0,
  rowListHeight: 72,
  rows: [],

  /**
   * Resize the panel to accomodate all rows.
   */
  resize: function() {
    var diff = this.rowsHeight - this.rowListHeight;
    if (diff != 0 && (this.rowListHeight + diff > 72)) {
      window.resizeBy(0, diff);
      this.rowListHeight += diff;
    }
  },

  /**
   * Remove a row from the list, as when a download is canceled, or
   * the the number of rows has exceeded the max allowed.
   *
   * @param {DownloadRow} row Row to be removed.
   * @private
   */
  remove: function(row) {
    this.rows.splice(this.rows.indexOf(row), 1);

    this.numRows--;
    this.rowsHeight -= row.element.offsetHeight;
    this.resize();

    this.element.removeChild(row.element);
    row.element.row = null;
  },

  removeList: function(rows) {
    for (i = 0; i < rows.length; i++) {
      this.remove(rows[i]);
    }
  },

  updateList: function(results) {
    for (var i = 0; i < results.length; i++) {
      this.update(results[i]);
    }
  },

  /**
   * Append a new row to the list, removing the last row if we exceed the
   * maximum allowed.
   * @param {DownloadRow} row Row to be removed.
   */
  append: function(row) {
    this.rows.push(row);

    var elem = row.element;
    var list = this.element;
    if (list.firstChild) {
      list.insertBefore(elem, list.firstChild);
    } else {
      list.appendChild(elem);
    }

    this.rowsHeight += elem.offsetHeight;

    this.numRows++;
    // We display no more than 5 elements.
    if (this.numRows > 5)
      this.remove(list.lastChild.row);

    this.resize();
  },

  getRow: function(path) {
    for (var i = 0; i < this.rows.length; i++) {
      if (this.rows[i].path == path)
        return this.rows[i];
    }
  },

  /**
   * Returns the list of rows that are not in the results array.
   * @param {Array} results Array of JSONified DownloadItems.
   */
  findMissing: function(results) {
    var removeList = [];

    for (var i = 0; i < this.rows.length; i++) {
      var row = this.rows[i];
      var found = false;
      for (var j = 0; j < results.length; j++) {
        if (row.path == results[j].file_path) {
          found = true;
          break;
        }
      }
      if (!found)
        removeList.push(row);
    }
    return removeList;
  },

  /**
   * Handle list callback with list of DownloadItems.
   * @param {Array} results Array of JSONified DownloadItems.
   */
  list: function(results) {
    var rows = this.findMissing(results);
    this.updateList(results);
    this.removeList(rows);
  },

  /**
   * Handle update of a DownloadItem we're observing.
   * @param {Object} result JSON representation of DownloadItem.
   */
  update: function(result) {
    var row = this.getRow(result.file_path);
    if (!row && !DiscardResult(result))
      row = new DownloadRow(this, result);

    row && row.update(result);
  },
};

document.addEventListener('DOMContentLoaded', init);
