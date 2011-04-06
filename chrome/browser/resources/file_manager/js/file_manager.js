// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * FileManager constructor.
 *
 * FileManager objects encapsulate the functionality of the file selector
 * dialogs, as well as the full screen file manager application (though the
 * latter is not yet implemented).
 *
 * @param {HTMLElement} dialogDom The DOM node containing the prototypical
 *     dialog UI.
 * @param {DOMFileSystem} filesystem The HTML5 filesystem object representing
 *     the root filesystem for the new FileManager.
 * @param {Object} params A map of parameter names to values controlling the
 *     appearance of the FileManager.  Names are:
 *     - type: A value from FileManager.DialogType defining what kind of
 *       dialog to present.  Defaults to SELECT_OPEN_FILE.
 *     - title: The title for the dialog.  Defaults to a localized string based
 *       on the dialog type.
 */
function FileManager(dialogDom, filesystem, params) {
  console.log('Init FileManager: ' + dialogDom);

  this.dialogDom_ = dialogDom;
  this.filesystem_ = filesystem;
  this.params_ = params || {type: FileManager.DialogType.SELECT_OPEN_FILE};

  this.document_ = dialogDom.ownerDocument;
  this.dialogType_ = this.params_.type;

  // DirectoryEntry representing the current directory of the dialog.
  this.currentDirEntry_ = null;

  this.addEventListener('directory-changed',
                        this.onDirectoryChanged_.bind(this));
  this.addEventListener('selection-summarized',
                        this.onSelectionSummarized_.bind(this));

  this.initDom_();
  this.initDialogType_();

  this.changeDirectory('/');
  this.summarizeSelection_();
  this.updatePreview_();
}

FileManager.prototype = {
  __proto__: cr.EventTarget.prototype
};

// Anonymous "namespace".
(function() {

  // Private variables and helper functions.

  /**
   * Directory where we can find icons.
   */
  const ICON_PATH = 'images/';

  /**
   * Unicode codepoint for 'BLACK RIGHT-POINTING SMALL TRIANGLE'.
   */
  const RIGHT_TRIANGLE = '\u25b8';

  /**
   * Translated strings.
   */
  var localStrings;

  /**
   * Map of icon types to regular expressions.
   *
   * The first regexp to match the file name determines the icon type
   * assigned to dom elements for a file.  Order of evaluation is not
   * defined, so don't depend on it.
   */
  const iconTypes = {
    'image': /\.(png|gif|jpe?g)$/i,
    // TODO(rginda): Uncomment these rules when we get the graphic assets.
    // 'document' : /\.(txt|doc|docx|wpd|odt|ps)$/i,
    // 'pdf' : /\.(pdf)$/i,
    // 'drawing': /\.(svg)$/i,
    // 'spreadsheet': /\.(xls|xlsx)$/i,
    // 'presentation': /\.(ppt|pptx)$/i
  };

  /**
   * Return a translated string.
   *
   * Wrapper function to make dealing with translated strings more concise.
   * Equivilant to localStrings.getString(id).
   *
   * @param {string} id The id of the string to return.
   * @return {string} The translated string.
   */
  function str(id) {
    return localStrings.getString(id);
  }

  /**
   * Return a translated string with arguments replaced.
   *
   * Wrapper function to make dealing with translated strings more concise.
   * Equivilant to localStrings.getStringF(id, ...).
   *
   * @param {string} id The id of the string to return.
   * @param {...string} The values to replace into the string.
   * @return {string} The translated string with replaced values.
   */
  function strf(id, var_args) {
    return localStrings.getStringF.apply(localStrings, arguments);
  }

  /**
   * Get the icon type for a given Entry.
   *
   * @param {Entry} entry An Entry subclass (FileEntry or DirectoryEntry).
   * @return {string} One of the keys from FileManager.iconTypes, or
   *     'unknown'.
   */
  function getIconType(entry) {
    if (entry.isDirectory)
      return 'folder';

    for (var name in this.iconTypes) {
      var value = this.iconTypes[name];

      if (value instanceof RegExp) {
        if (value.test(entry.name))
          return name;
      } else if (typeof value == 'function') {
        try {
          if (value(entry))
            return name;
        } catch (ex) {
          console.error('Caught exception while evaluating iconType: ' +
                        name, ex);
        }
      } else {
        console.log('Unexpected value in iconTypes[' + name + ']: ' + value);
      }
    }

    return 'unknown';
  }

  /**
   * Call an asynchronous method on dirEntry, batching multiple callers.
   *
   * This batches multiple callers into a single invocation, calling all
   * interested parties back when the async call completes.  If the async call
   * has already been made this will invoke the successCallback synchronously.
   *
   * The Entry method to be invoked should take two callbacks as parameters
   * (one for success and one for failure), and it should invoke those
   * callbacks with a single parameter representing the result of the call.
   * Example methods are Entry.getMetadata() and FileEntry.file().
   *
   * Warning: Because this method caches the first result, subsequent changes
   * to the entry will not be visible to callers.
   *
   * Error results are never cached.
   *
   * @param {DirectoryEntry} dirEntry The DirectoryEntry to apply the method
   *     to.
   * @param {string} methodName The name of the method to dispatch.
   * @param {Function(*)} successCallback The function to invoke if the method
   *     succeeds.  The result of the method will be the one parameter to this
   *     callback.
   * @param {Function(*)} opt_errorCallback The function to invoke if the
   *     method fails.  The result of the method will be the one parameter to
   *     this callback.  If not provided, the default errorCallback will throw
   *     an exception.
   */
  function batchAsyncCall(entry, methodName, successCallback,
                          opt_errorCallback) {
    var resultCache = methodName + '_resultCache_';

    if (entry[resultCache]) {
      // The result cache for this method already exists.  Just invoke the
      // successCallback with the result of the previuos call.
      successCallback(entry[resultCache]);
      return;
    }

    if (!opt_errorCallback) {
      opt_errorCallback = util.ferr('Error calling ' + methodName + ' for: ' +
                                    entry.fullPath);
    }

    var observerList = methodName + '_observers_';

    if (entry[observerList]) {
      // The observer list already exists, indicating we have a pending call
      // out to this method.  Add this caller to the list of observers and
      // bail out.
      entry[observerList].push([successCallback, opt_errorCallback]);
      return;
    }

    entry[observerList] = [[successCallback, opt_errorCallback]];

    function onComplete(success, result) {
      if (success)
        entry[resultCache] = result;

      for (var i = 0; i < entry[observerList].length; i++) {
        entry[observerList][i][success ? 0 : 1](result);
      }

      delete entry[observerList];
    };

    entry[methodName](function(rv) { onComplete(true, rv) },
                      function(rv) { onComplete(false, rv) });
  }

  /**
   * Get the size of a file, caching the result.
   *
   * When this method completes, the fileEntry object will get a
   * 'cachedSize_' property (if it doesn't already have one) containing the
   * size of the file in bytes.
   *
   * Note that if the size of the file is already known, the successCallback
   * will be invoked synchronously.
   */
  function cacheFileSize(fileEntry, successCallback) {
    if ('cachedSize_' in fileEntry) {
      if (successCallback)
        successCallback(fileEntry);
      return;
    }

    batchAsyncCall(fileEntry, 'file', function(file) {
      fileEntry.cachedSize_ = file.size;
      if (successCallback)
        successCallback(fileEntry);
    });
  }

  // Public statics.

  /**
   * List of dialog types.
   *
   * Keep this in sync with FileManagerDialog::GetDialogTypeAsString.
   *
   * @enum {string}
   */
  FileManager.DialogType = {
    SELECT_FOLDER: 'folder',
    SELECT_SAVEAS_FILE: 'saveas-file',
    SELECT_OPEN_FILE: 'open-file',
    SELECT_OPEN_MULTI_FILE: 'open-multi-file'
  };

  /**
   * Load translated strings.
   */
  FileManager.initStrings = function(callback) {
    chrome.fileBrowserPrivate.getStrings(function(strings) {
      localStrings = new LocalStrings(strings);
      cr.initLocale(strings);

      if (callback)
        callback();
    });
  };

  // Instance methods.

  /**
   * One-time initialization of various DOM nodes.
   */
  FileManager.prototype.initDom_ = function() {
    // Cache nodes we'll be manipulating.
    this.previewImage_ = this.dialogDom_.querySelector('.preview-img');
    this.previewFilename_ = this.dialogDom_.querySelector('.preview-filename');
    this.previewSummary_ = this.dialogDom_.querySelector('.preview-summary');
    this.filenameInput_ = this.dialogDom_.querySelector('.filename-input');
    this.okButton_ = this.dialogDom_.querySelector('.ok');
    this.cancelButton_ = this.dialogDom_.querySelector('.cancel');

    if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      this.filenameInput_.addEventListener(
          'keyup', this.onFilenameInputKeyUp_.bind(this));
      this.filenameInput_.addEventListener(
          'focus', this.onFilenameInputFocus_.bind(this));
    } else {
      var label = this.dialogDom_.querySelector('.filename-label');
      label.style.visibility = 'hidden';
      this.filenameInput_.style.visibility = 'hidden';
    }

    this.okButton_.addEventListener('click', this.onOk_.bind(this));
    this.cancelButton_.addEventListener('click', this.onCancel_.bind(this));

    // Populate the static localized strings.
    i18nTemplate.process(this.document_, localStrings.templateData);

    // Set up the detail table.
    var dataModel = new cr.ui.table.TableDataModel([]);
    dataModel.idField = 'name';
    dataModel.defaultSortField = 'name';

    var columns = [
        new cr.ui.table.TableColumn('name', str('NAME_COLUMN_LABEL'), 64),
        new cr.ui.table.TableColumn('cachedSize_',
                                    str('SIZE_COLUMN_LABEL'), 15),
        new cr.ui.table.TableColumn('cachedMtime_',
                                    str('DATE_COLUMN_LABEL'), 21)
    ];

    columns[0].renderFunction = this.renderName_.bind(this);
    columns[1].renderFunction = this.renderSize_.bind(this);
    columns[2].renderFunction = this.renderDate_.bind(this);

    this.table = this.dialogDom_.querySelector('.detail-table');
    cr.ui.Table.decorate(this.table);
    this.table.dataModel = dataModel;
    this.table.columnModel = new cr.ui.table.TableColumnModel(columns);

    if (this.dialogType_ != FileManager.DialogType.SELECT_OPEN_MULTI_FILE) {
      this.table.selectionModel = new cr.ui.table.TableSingleSelectionModel();
    }

    this.table.addEventListener(
        'dblclick', this.onDetailDoubleClick_.bind(this));
    this.table.selectionModel.addEventListener(
        'change', this.onDetailSelectionChanged_.bind(this));
  };

  /**
   * Tweak the UI to become a particular kind of dialog, as determined by the
   * dialog type parameter passed to the constructor.
   */
  FileManager.prototype.initDialogType_ = function() {
    var defaultTitle;
    var okLabel = str('OPEN_LABEL');

    switch (this.dialogType_) {
      case FileManager.DialogType.SELECT_FOLDER:
        defaultTitle = str('SELECT_FOLDER_TITLE');
        break;

      case FileManager.DialogType.SELECT_OPEN_FILE:
        defaultTitle = str('SELECT_OPEN_FILE_TITLE');
        break;

      case FileManager.DialogType.SELECT_OPEN_MULTI_FILE:
        defaultTitle = str('SELECT_OPEN_MULTI_FILE_TITLE');
        break;

      case FileManager.DialogType.SELECT_SAVEAS_FILE:
        defaultTitle = str('SELECT_SAVEAS_FILE_TITLE');
        okLabel = str('SAVE_LABEL');
        break;

      default:
        throw new Error('Unknown dialog type: ' + this.dialogType_);
    }

    this.okButton_.textContent = okLabel;

    dialogTitle = this.params_.title || defaultTitle;
    this.dialogDom_.querySelector('.dialog-title').textContent = dialogTitle;
  };

  /**
   * Render the Name column of the detail table.
   *
   * Invoked by cr.ui.Table when a file needs to be rendered.
   *
   * @param {Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {cr.ui.Table} table The table doing the rendering.
   */
  FileManager.prototype.renderName_ = function(entry, columnId, table) {
    var container = this.document_.createElement('div');

    var icon = this.document_.createElement('div');
    icon.className = 'detail-icon'
    icon.setAttribute('iconType', getIconType(entry));
    container.appendChild(icon);

    var label = this.document_.createElement('div');
    label.className = 'detail-name';
    label.textContent = entry.name;
    container.appendChild(label);

    return container;
  };

  /**
   * Render the Size column of the detail table.
   *
   * @param {Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {cr.ui.Table} table The table doing the rendering.
   */
  FileManager.prototype.renderSize_ = function(entry, columnId, table) {
    var div = this.document_.createElement('div');
    div.className = 'detail-size';

    if (entry.isFile) {
      div.textContent = '...';
      cacheFileSize(entry, function(fileEntry) {
        div.textContent = cr.locale.bytesToSi(fileEntry.cachedSize_);
      });
    } else {
      // No size for a directory, -1 ensures it's sorted before 0 length files.
      entry.cachedSize_ = -1;
      div.textContent = '';
    }

    return div;
  };

  /**
   * Render the Date column of the detail table.
   *
   * @param {Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {cr.ui.Table} table The table doing the rendering.
   */
  FileManager.prototype.renderDate_ = function(entry, columnId, table) {
    var div = this.document_.createElement('div');
    div.className = 'detail-date';
    div.textContent = '...';

    if (entry.isFile) {
      batchAsyncCall(entry, 'file', function(file) {
        entry.cachedMtime_ = file.lastModifiedDate.getTime();
        div.textContent = cr.locale.formatDate(file.lastModifiedDate,
                                               str('SHORT_DATE_FORMAT'));
      });
    } else {
      batchAsyncCall(entry, 'getMetadata', function(metadata) {
        entry.cachedMtime_ = metadata.modificationTime.getTime();
        div.textContent = cr.locale.formatDate(metadata.modificationTime,
                                               str('SHORT_DATE_FORMAT'));
      });
    }

    return div;
  };

  /**
   * Compute summary information about the current selection.
   *
   * This method dispatches the 'selection-summarized' event when it completes.
   * Depending on how many of the selected files already have known sizes, the
   * dispatch may happen immediately, or after a number of async calls complete.
   */
  FileManager.prototype.summarizeSelection_ = function() {
    var selection = this.selection = {
      entries: [],
      leadEntry: null,
      totalCount: 0,
      fileCount: 0,
      directoryCount: 0,
      bytes: 0,
      iconType: null,
    };

    this.previewSummary_.textContent = str('COMPUTING_SELECTION');

    var selectedIndexes = this.table.selectionModel.selectedIndexes;
    if (!selectedIndexes.length) {
      cr.dispatchSimpleEvent(this, 'selection-summarized');
      return;
    }

    var fileCount = 0;
    var byteCount = 0;
    var pendingFiles = [];

    for (var i = 0; i < selectedIndexes.length; i++) {
      var entry = this.table.dataModel.item(selectedIndexes[i]);

      selection.entries.push(entry);

      if (selection.iconType == null) {
        selection.iconType = getIconType(entry);
      } else if (selection.iconType != 'unknown') {
        var iconType = getIconType(entry);
        if (selection.iconType != iconType)
          selection.iconType = 'unknown';
      }

      selection.totalCount++;

      if (entry.isFile) {
        if (!('cachedSize_' in entry)) {
          // Any file that hasn't been rendered may be missing its cachedSize_
          // property.  For example, visit a large file list, and press ctrl-a
          // to select all.  In this case, we need to asynchronously get the
          // sizes for these files before telling the world the selection has
          // been summarized.  See the 'computeNextFile' logic below.
          pendingFiles.push(entry);
          continue;
        } else {
          selection.bytes += entry.cachedSize_;
        }
        selection.fileCount += 1;
      } else {
        selection.directoryCount += 1;
      }
    }

    var leadIndex = this.table.selectionModel.leadIndex;
    if (leadIndex > -1) {
      selection.leadEntry = this.table.dataModel.item(leadIndex);
    } else {
      selection.leadEntry = selection.entries[0];
    }

    var self = this;

    function computeNextFile(fileEntry) {
      if (fileEntry) {
        // We're careful to modify the 'selection', rather than 'self.selection'
        // here, just in case the selection has changed since this summarization
        // began.
        selection.bytes += fileEntry.cachedSize_;
      }

      if (pendingFiles.length) {
        cacheFileSize(pendingFiles.pop(), computeNextFile);
      } else {
        self.dispatchEvent(new cr.Event('selection-summarized'));
      }
    };

    computeNextFile();
  };

  /**
   * Update the breadcrumb display to reflect the current directory.
   */
  FileManager.prototype.updateBreadcrumbs_ = function() {
    var bc = this.dialogDom_.querySelector('.breadcrumbs');
    bc.innerHTML = '';

    var fullPath = this.currentDirEntry_.fullPath.replace(/\/$/, '');
    var pathNames = fullPath.split('/');
    var path = '';

    for (var i = 0; i < pathNames.length; i++) {
      var pathName = pathNames[i];
      path += pathName + '/';

      var div = this.document_.createElement('div');
      div.className = 'breadcrumb-path';
      div.textContent = pathNames[i] || str('ROOT_DIRECTORY_LABEL');
      div.path = path;
      div.addEventListener('click', this.onBreadcrumbClick_.bind(this));
      bc.appendChild(div);

      if (i == pathNames.length - 1) {
        div.classList.add('breadcrumb-last');
      } else {
        var spacer = this.document_.createElement('div');
        spacer.className = 'breadcrumb-spacer';
        spacer.textContent = RIGHT_TRIANGLE;
        bc.appendChild(spacer);
      }
    }
  };

  /**
   * Update the preview panel to display a given entry.
   *
   * The selection summary line is handled by the onSelectionSummarized handler
   * rather than this function, because summarization may not complete quickly.
   */
  FileManager.prototype.updatePreview_ = function() {
    // Clear the preview image first, in case the thumbnail takes long to load.
    this.previewImage_.src = '';
    // The transparent-background class is used to display the checkerboard
    // background for image thumbnails.  We don't want to display it for
    // non-thumbnail preview images.
    this.previewImage_.classList.remove('transparent-background');
    // The multiple-selected class indicates that more than one entry is
    // selcted.
    this.previewImage_.classList.remove('multiple-selected');

    if (!this.selection.totalCount) {
      this.previewFilename_.textContent = '';
      return;
    }

    this.previewFilename_.textContent = this.selection.leadEntry.name;

    var entry = this.selection.leadEntry;
    var iconType = getIconType(entry);
    if (iconType != 'image') {
      // Not an image, display a canned clip-art graphic.
      this.previewImage_.src = ICON_PATH + 'preview-' + iconType + '.png';
    } else {
      // File is an image, fetch the thumbnail.
      var fileManager = this;

      batchAsyncCall(entry, 'file', function(file) {
        var reader = new FileReader();

        reader.onerror = util.ferr('Error reading preview: ' + entry.fullPath);
        reader.onloadend = function(e) {
          fileManager.previewImage_.src = this.result;
          fileManager.previewImage_.classList.add('transparent-background');
          if (fileManager.selection.totalCount > 1)
            fileManager.previewImage_.classList.add('multiple-selected');
        };

        reader.readAsDataURL(file);
      });
    }
  };

  /**
   * Change the current directory.
   *
   * Dispatches the 'directory-changed' event when the directory is successfully
   * changed.
   *
   * @param {string} path The absolute path to the new directory.
   */
  FileManager.prototype.changeDirectory = function(path) {
    var self = this;

    function onPathFound(dirEntry) {
      if (self.currentDirEntry_ &&
          self.currentDirEntry_.fullPath == dirEntry.fullPath) {
        // Directory didn't actually change.
        return;
      }

      var e = new cr.Event('directory-changed');
      e.previousDirEntry = self.currentDirEntry_;
      e.newDirEntry = dirEntry;
      self.currentDirEntry_ = dirEntry;
      self.dispatchEvent(e);
    };

    if (path == '/')
      return onPathFound(this.filesystem_.root);

    this.filesystem_.root.getDirectory(
        path, {create: false}, onPathFound,
        util.ferr('Error changing directory to: ' + path));
  };

  /**
   * Update the selection summary UI when the selection summarization completes.
   */
  FileManager.prototype.onSelectionSummarized_ = function() {
    if (this.selection.totalCount == 0) {
      this.previewSummary_.textContent = str('NOTHING_SELECTED');

    } else if (this.selection.totalCount == 1) {
      this.previewSummary_.textContent =
        strf('ONE_FILE_SELECTED', cr.locale.bytesToSi(this.selection.bytes));

    } else {
      this.previewSummary_.textContent =
        strf('MANY_FILES_SELECTED', this.selection.totalCount,
             cr.locale.bytesToSi(this.selection.bytes));
    }
  };

  /**
   * Handle a click event on a breadcrumb element.
   *
   * @param {Event} event The click event.
   */
  FileManager.prototype.onBreadcrumbClick_ = function(event) {
    this.changeDirectory(event.srcElement.path);
  };

  /**
   * Update the UI when the selection model changes.
   *
   * @param {cr.Event} event The change event.
   */
  FileManager.prototype.onDetailSelectionChanged_ = function(event) {
    var selectable;

    this.summarizeSelection_();

    if (this.dialogType_ == FileManager.DialogType.SELECT_FOLDER) {
      selectable = this.selection.directoryCount == 1 &&
          this.selection.fileCount == 0;
    } else if (this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FILE) {
      selectable = (this.selection.directoryCount == 0 &&
                    this.selection.fileCount == 1);
    } else if (this.dialogType_ ==
               FileManager.DialogType.SELECT_OPEN_MULTI_FILE) {
      selectable = (this.selection.directoryCount == 0 &&
                    this.selection.fileCount >= 1);
    } else if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      selectable = this.selection.leadEntry && this.selection.leadEntry.isFile;
      if (selectable)
        this.filenameInput_.value = this.selection.leadEntry.name;
    } else {
      throw new Error('Unknown dialog type');
    }

    this.okButton_.disabled = !selectable;
    this.updatePreview_();
  };

  /**
   * Handle a double-click event on an entry in the detail list.
   *
   * @param {Event} event The click event.
   */
  FileManager.prototype.onDetailDoubleClick_ = function(event) {
    var i = this.table.selectionModel.leadIndex;
    var entry = this.table.dataModel.item(i);

    if (entry.isDirectory)
      return this.changeDirectory(entry.fullPath);

  };

  /**
   * Update the UI when the current directory changes.
   *
   * @param {cr.Event} event The directory-changed event.
   */
  FileManager.prototype.onDirectoryChanged_ = function(event) {
    var self = this;
    var reader;

    function onReadSome(entries) {
      if (entries.length == 0)
      return;

      // Splice takes the to-be-spliced-in array as individual parameters,
      // rather than as an array, so we need to perform some acrobatics...
      var spliceArgs = [].slice.call(entries);
      spliceArgs.unshift(0, 0);  // index, deleteCount
      self.table.dataModel.splice.apply(self.table.dataModel, spliceArgs);

      // Keep reading until entries.length is 0.
      reader.readEntries(onReadSome);
    };

    // Clear the table first.
    this.table.dataModel.splice(0, this.table.dataModel.length);

    this.updateBreadcrumbs_();

    reader = this.currentDirEntry_.createReader();
    reader.readEntries(onReadSome);
  };

  FileManager.prototype.onFilenameInputKeyUp_ = function(event) {
    this.okButton_.disabled = this.filenameInput_.value.length == 0;
  };

  FileManager.prototype.onFilenameInputFocus_ = function(event) {
    var input = this.filenameInput_;

    // On focus we want to select everything but the extension, but
    // Chrome will select-all after the focus event completes.  We
    // schedule a timeout to alter the focus after that happens.
    setTimeout(function() {
        var selectionEnd = input.value.lastIndexOf('.');
        if (selectionEnd == -1) {
          input.select();
        } else {
          input.selectionStart = 0;
          input.selectionEnd = selectionEnd;
        }
    }, 0);
  };

  /**
   * Handle a click of the cancel button.
   *
   * @param {Event} event The click event.
   */
  FileManager.prototype.onCancel_ = function(event) {
    chrome.fileBrowserPrivate.cancelDialog();
  };

  /**
   * Handle a click of the ok button.
   *
   * The ok button has different UI labels depending on the type of dialog, but
   * in code it's always referred to as 'ok'.
   *
   * @param {Event} event The click event.
   */
  FileManager.prototype.onOk_ = function(event) {
    if (this.dialogType_ == FileManager.DialogType.SELECT_SAVEAS_FILE) {
      // Save-as doesn't require a valid selection from the list, since
      // we're going to take the filename from the text input.
      var filename = this.filenameInput_.value;
      if (!filename)
        throw new Error('Missing filename!');

      chrome.fileBrowserPrivate.selectFile(this.currentDirEntry_.fullPath +
                                           '/' + filename, 0);
      return;
    }

    var ary = [];
    var selectedIndexes = this.table.selectionModel.selectedIndexes;

    // All other dialog types require at least one selected list item.
    // The logic to control whether or not the ok button is enabled should
    // prevent us from ever getting here, but we sanity check to be sure.
    if (!selectedIndexes.length)
      throw new Error('Nothing selected!');

    for (var i = 0; i < selectedIndexes.length; i++) {
      var entry = this.table.dataModel.item(selectedIndexes[i]);
      if (!entry) {
        console.log('Error locating selected file at index: ' + i);
        continue;
      }

      ary.push(entry.fullPath);
    }

    // Multi-file selection has no other restrictions.
    if (this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FILES) {
      chrome.fileBrowserPrivate.selectFiles(ary);
      return;
    }

    // Everything else must have exactly one.
    if (ary.length > 1)
      throw new Error('Too many files selected!');

    if (this.dialogType_ == FileManager.DialogType.SELECT_FOLDER) {
      if (!this.selection.leadEntry.isDirectory)
        throw new Error('Selected entry is not a folder!');
    } else if (this.dialogType_ == FileManager.DialogType.SELECT_OPEN_FILE) {
      if (!this.selection.leadEntry.isFile) {
        throw new Error('Selected entry is not a file!');
    }

    chrome.fileBrowserPrivate.selectFile(ary[0], 0);
  };

})();
