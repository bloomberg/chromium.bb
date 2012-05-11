// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var MAX_DRAG_THUMBAIL_COUNT = 4;

/**
 * Global (placed in the window object) variable name to hold internal
 * file dragging information. Needed to show visual feedback while dragging
 * since DataTransfer object is in protected state. Reachable from other
 * file manager instances.
 */
var DRAG_AND_DROP_GLOBAL_DATA = '__drag_and_drop_global_data';

/**
 * TODO(olege): Fix style warnings.
 */
function FileTransferController(fileList,
                                fileListSelection,
                                dragNodeConstructor,
                                copyManager,
                                directoryModel) {
  this.fileList_ = fileList;
  this.fileListSelection_ = fileListSelection;
  this.dragNodeConstructor_ = dragNodeConstructor;
  this.copyManager_ = copyManager;
  this.directoryModel_ = directoryModel;

  this.fileListSelection_.addEventListener('change',
      this.onSelectionChanged_.bind(this));

  /**
   * DOM elements to represent selected files in drag operation.
   * @type {Array.<Element>}
   */
  this.dragNodes_ = [];

  /**
   * File objects for seletced files.
   * @type {Array.<File>}
   */
  this.files_ = [];

}

FileTransferController.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * @param {cr.ui.List} list Items in the list will be draggable.
   */
  attachDragSource: function(list) {
    list.style.webkitUserDrag = 'element';
    list.addEventListener('dragstart', this.onDragStart_.bind(this, list));
    list.addEventListener('dragend', this.onDragEnd_.bind(this, list));
  },

  /**
   * @param {cr.ui.List} list List itself and its directory items will could
   *                          be drop target.
   * @param {boolean=} opt_onlyIntoDirectories If true only directory list
   *     items could be drop targets. Otherwise any other place of the list
   *     accetps files (putting it into the current directory).
   */
  attachDropTarget: function(list, opt_onlyIntoDirectories) {
    list.addEventListener('dragover', this.onDragOver_.bind(this,
                          !!opt_onlyIntoDirectories, list));
    list.addEventListener('dragenter', this.onDragEnterList_.bind(this, list));
    list.addEventListener('dragleave', this.onDragLeave_.bind(this, list));
    list.addEventListener('drop', this.onDrop_.bind(this,
                          !!opt_onlyIntoDirectories));
  },

  attachBreadcrumbsDropTarget: function(breadcrumbs) {
    breadcrumbs.addEventListener('dragover',
        this.onDragOver_.bind(this, true, null));
    breadcrumbs.addEventListener('dragenter',
        this.onDragEnterBreadcrumbs_.bind(this, breadcrumbs));
    breadcrumbs.addEventListener('dragleave',
        this.onDragLeave_.bind(this, null));
    breadcrumbs.addEventListener('drop', this.onDrop_.bind(this, true));
  },

  /**
   * Attach handlers of copy, cut and paste operations to the document.
   * @param {HTMLDocument} doc Command dispatcher.
   */
  attachCopyPasteHandlers: function(doc) {
    this.document_ = doc;
    doc.addEventListener('beforecopy', this.onBeforeCopy_.bind(this));
    doc.addEventListener('copy', this.onCopy_.bind(this));
    doc.addEventListener('beforecut', this.onBeforeCut_.bind(this));
    doc.addEventListener('cut', this.onCut_.bind(this));
    doc.addEventListener('beforepaste', this.onBeforePaste_.bind(this));
    doc.addEventListener('paste', this.onPaste_.bind(this));
    this.copyCommand_ = doc.querySelector('command#copy');
  },

  /**
   * Write the current selection to system clipboard.
   *
   * @param {DataTransfer} dataTransfer DataTransfer from the event.
   * @param {string} effectAllowed Value must be valid for the
   *     |dataTransfer.effectAllowed| property ('move', 'copy', 'copyMove').
   */
  cutOrCopy: function(dataTransfer, effectAllowed) {
    var directories = [];
    var files = [];
    var entries = this.selectedEntries_;
    for (var i = 0; i < entries.length; i++) {
      (entries[i].isDirectory ? directories : files).push(entries[i].fullPath);
    }

    // Tag to check it's filemanager data.
    dataTransfer.setData('fs/tag', 'filemanager-data');

    dataTransfer.setData('fs/isOnGData', this.isOnGData);
    dataTransfer.setData('fs/sourceDir',
                         this.currentDirectory.fullPath);
    dataTransfer.setData('fs/directories', directories.join('\n'));
    dataTransfer.setData('fs/files', files.join('\n'));
    dataTransfer.effectAllowed = effectAllowed;
    dataTransfer.setData('fs/effectallowed', effectAllowed);

    for (var i = 0; i < this.files_.length; i++) {
      dataTransfer.items.add(this.files_[i]);
    }
  },

  /**
   * Extracts source root from the |dataTransfer| object.
   * @param {DataTransfer} dataTransfer DataTransfer object from the event.
   * @return {string} Path or empty string (if unknown).
   */
  getSourceRoot_: function(dataTransfer) {
    var sourceDir = dataTransfer.getData('fs/sourceDir');
    if (sourceDir)
      return DirectoryModel.getRootPath(sourceDir);

    // |dataTransfer| in protected mode.
    if (window[DRAG_AND_DROP_GLOBAL_DATA])
      return window[DRAG_AND_DROP_GLOBAL_DATA].sourceRoot;

    // Dragging from other tabs/windows.
    var views = chrome && chrome.extension ? chrome.extension.getViews() : [];
    for (var i = 0; i < views.length; i++) {
      if (views[i][DRAG_AND_DROP_GLOBAL_DATA])
        return views[i][DRAG_AND_DROP_GLOBAL_DATA].sourceRoot;
    }

    // Unknown source.
    return '';
  },

  /**
   * Queue up a file copy operation based on the current system clipboard.
   * @param {DataTransfer} dataTransfer System data transfer object.
   * @param {string=} opt_destinationPath Paste destination.
   * @param {string=} opt_effect Desired drop/paste effect. Could be
   *     'move'|'copy' (default is copy). Ignored if conflicts with
   *     |dataTransfer.effectAllowed|.
   */
  paste: function(dataTransfer, opt_destinationPath, opt_effect) {
    var destinationPath = opt_destinationPath ||
                          this.directoryModel_.getCurrentDirPath();
    // effectAllowed set in copy/pase handlers stay uninitialized. DnD handlers
    // work fine.
    var effectAllowed = dataTransfer.effectAllowed != 'uninitialized' ?
        dataTransfer.effectAllowed : dataTransfer.getData('fs/effectallowed');

    var toMove = effectAllowed == 'move' ||
        (effectAllowed == 'copyMove' && opt_effect == 'move');

    var operationInfo = {
      isCut: String(toMove),
      isOnGData: dataTransfer.getData('fs/isOnGData'),
      sourceDir: dataTransfer.getData('fs/sourceDir'),
      directories: dataTransfer.getData('fs/directories'),
      files: dataTransfer.getData('fs/files')
    };

    if (!toMove || operationInfo.sourceDir != destinationPath) {
      var targetOnGData = DirectoryModel.getRootType(destinationPath) ==
          DirectoryModel.RootType.GDATA;
      this.copyManager_.paste(operationInfo,
                              destinationPath,
                              targetOnGData);
    } else {
      console.log('Ignore move into the same folder');
    }

    return toMove ? 'move' : 'copy';
  },

  onDragStart_: function(list, event) {
    var dt = event.dataTransfer;
    var doc = list.ownerDocument;
    var container = doc.querySelector('#drag-image-container');
    var length = this.dragNodes_.length;
    for (var i = 0; i < length; i++) {
      var listItem = this.dragNodes_[i];
      listItem.selected = true;
      listItem.style.zIndex = length - i;
      container.appendChild(listItem);
    }
    dt.setDragImage(container, 0, 0);

    if (this.canCopyOrDrag_(dt)) {
      if (this.canCutOrDrag_(dt))
        this.cutOrCopy(dt, 'copyMove');
      else
        this.cutOrCopy(dt, 'copy');
    } else {
      event.preventDefault();
    }
    window[DRAG_AND_DROP_GLOBAL_DATA] = {
      sourceRoot: this.directoryModel_.getCurrentRootPath()
    };
  },

  onDragEnd_: function(list, event) {
    var doc = list.ownerDocument;
    var container = doc.querySelector('#drag-image-container');
    container.textContent = '';
    this.setDropTarget_(null);
    this.setScrollSpeed_(null, 0);
    delete window[DRAG_AND_DROP_GLOBAL_DATA];
  },

  onDragOver_: function(onlyIntoDirectories, list, event) {
    if (list) {
      // Scroll the list if mouse close to the top or the bottom.
      var rect = list.getBoundingClientRect();
      if (event.clientY - rect.top < rect.bottom - event.clientY) {
        this.setScrollSpeed_(list,
            -this.calculateScrollSpeed_(event.clientY - rect.top));
      } else {
        this.setScrollSpeed_(list,
            this.calculateScrollSpeed_(rect.bottom - event.clientY));
      }
    }
    event.preventDefault();
    var path = this.destinationPath_ ||
        (!onlyIntoDirectories && this.directoryModel_.getCurrentDirPath());
    event.dataTransfer.dropEffect = this.selectDropEffect_(event, path);
    event.preventDefault();
  },

  onDragEnterList_: function(list, event) {
    this.dragEnterCount_++;
    var item = list.getListItemAncestor(event.target);
    item = item && list.isItem(item) ? item : null;
    if (item == this.dropTarget_)
      return;

    var entry = item && list.dataModel.item(item.listIndex);
    if (entry) {
      this.setDropTarget_(item, entry.isDirectory, event.dataTransfer,
          entry.fullPath);
    } else {
      this.setDropTarget_(null);
    }
  },

  onDragEnterBreadcrumbs_: function(breadcrumbs, event) {
    if (!event.target.classList.contains('breadcrumb-path'))
      return;
    this.dragEnterCount_++;

    var items = breadcrumbs.querySelectorAll('.breadcrumb-path');
    var path = this.directoryModel_.getCurrentRootPath();

    if (event.target != items[0]) {
      for (var i = 1; items[i - 1] != event.target; i++) {
        path += '/' + items[i].textContent;
      }
    }

    this.setDropTarget_(event.target, true, event.dataTransfer, path);
  },

  onDragLeave_: function(list, event) {
    if (this.dragEnterCount_-- == 0)
      this.setDropTarget_(null);
    if (event.target == list)
      this.setScrollSpeed_(list, 0);
  },

  onDrop_: function(onlyIntoDirectories, event) {
    if (onlyIntoDirectories && !this.dropTarget_)
      return;
    var destinationPath = this.destinationPath_ ||
                          this.directoryModel_.getCurrentDirPath();
    if (!this.canPasteOrDrop_(event.dataTransfer, destinationPath))
      return;
    event.preventDefault();
    this.paste(event.dataTransfer, destinationPath,
               this.selectDropEffect_(event, destinationPath));
    this.setDropTarget_(null);
    this.setScrollSpeed_(null, 0);
  },

  setDropTarget_: function(domElement, isDirectory, opt_dataTransfer,
                           opt_destinationPath) {
    if (this.dropTarget_ == domElement)
      return;

    /** @type {string?} */
    this.destinationPath_ = null;
    if (domElement) {
      if (isDirectory &&
          this.canPasteOrDrop_(opt_dataTransfer, opt_destinationPath)) {
        domElement.classList.add('accepts');
        this.destinationPath_ = opt_destinationPath;
      }
    } else {
      this.dragEnterCount_ = 0;
    }
    if (this.dropTarget_ && this.dropTarget_.classList.contains('accepts')) {
      var oldDropTarget = this.dropTarget_;
      var self = this;
      setTimeout(function() {
        if (oldDropTarget != self.dropTarget_)
          oldDropTarget.classList.remove('accepts');
      }, 0);
    }
    this.dropTarget_ = domElement;
    if (this.navigateTimer_ !== undefined) {
      clearTimeout(this.navigateTimer_);
      this.navigateTimer_ = undefined;
    }
    if (domElement && isDirectory && opt_destinationPath) {
      this.navigateTimer_ = setTimeout(function() {
        this.directoryModel_.changeDirectoryOrRoot(opt_destinationPath);
      }.bind(this), 2000);
    }
  },

  isDocumentWideEvent_: function(event) {
    return event.target == this.document_.body;
  },

  onCopy_: function(event) {
    if (!this.isDocumentWideEvent_(event) ||
        !this.canCopyOrDrag_()) {
      return;
    }
    event.preventDefault();
    this.cutOrCopy(event.clipboardData, 'copy');
    this.notify_('selection-copied');
  },

  onBeforeCopy_: function(event) {
    if (!this.isDocumentWideEvent_(event))
      return;

    // queryCommandEnabled returns true if event.returnValue is false.
    event.returnValue = !this.canCopyOrDrag_();
  },

  canCopyOrDrag_: function() {
    if (this.isOnGData &&
        this.directoryModel_.isOffline() &&
        !this.allGDataFilesAvailable)
      return false;
    return this.selectedEntries_.length > 0;
  },

  onCut_: function(event) {
    if (!this.isDocumentWideEvent_(event) ||
        !this.canCutOrDrag_()) {
      return;
    }
    event.preventDefault();
    this.cutOrCopy(event.clipboardData, 'move');
    this.notify_('selection-cut');
  },

  onBeforeCut_: function(event) {
    if (!this.isDocumentWideEvent_(event))
      return;
    // queryCommandEnabled returns true if event.returnValue is false.
    event.returnValue = !this.canCutOrDrag_();
  },

  canCutOrDrag_: function() {
    return !this.readonly && this.canCopyOrDrag_();
  },

  onPaste_: function(event) {
    // Need to update here since 'beforepaste' doesn't fire.
    if (!this.isDocumentWideEvent_(event) ||
        !this.canPasteOrDrop_(event.clipboardData)) {
      return;
    }
    event.preventDefault();
    var effect = this.paste(event.clipboardData);

    // On cut, we clear the clipboard after the file is pasted/moved so we don't
    // try to move/delete the original file again.
    if (effect == 'move') {
      this.simulateCommand_('cut', function(event) {
        event.preventDefault();
        event.clipboardData.setData('fs/clear', '');
      });
    }
  },

  onBeforePaste_: function(event) {
    if (!this.isDocumentWideEvent_(event))
      return;
    // queryCommandEnabled returns true if event.returnValue is false.
    event.returnValue = !this.canPasteOrDrop_(event.clipboardData);
  },

  canPasteOrDrop_: function(dataTransfer, opt_destinationPath) {
    var destinationPath = opt_destinationPath ||
                          this.directoryModel_.getCurrentDirPath();
    if (this.directoryModel_.isPathReadOnly(destinationPath)) {
      return false;
    }

    if (!dataTransfer.types || dataTransfer.types.indexOf('fs/tag') == -1)
      return false;  // Unsupported type of content.
    if (dataTransfer.getData('fs/tag') == '') {
      // Data protected. Other checks are not possible but it makes sense to
      // let the user try.
      return true;
    }

    var directories = dataTransfer.getData('fs/directories').split('\n').
                      filter(function(d) { return d != ''; });

    for (var i = 0; i < directories.length; i++) {
      if (destinationPath.substr(0, directories[i].length) == directories[i])
        return false;  // recursive paste.
    }

    return true;
  },

  queryPasteCommandEnabled: function() {
    // HACK(serya): return this.document_.queryCommandEnabled('paste')
    // should be used.
    var result;
    this.simulateCommand_('paste', function(event) {
      result = this.canPasteOrDrop_(event.clipboardData);
    }.bind(this));
    return result;
  },

  /**
   * Allows to simulate commands to get access to clipboard.
   * @param {string} command 'copy', 'cut' or 'paste'.
   * @param {Function} handler Event handler.
   */
  simulateCommand_: function(command, handler) {
    var iframe = this.document_.querySelector('#command-dispatcher');
    var doc = iframe.contentDocument;
    doc.addEventListener(command, handler);
    doc.execCommand(command);
    doc.removeEventListener(command, handler);
  },

  onSelectionChanged_: function(event) {
    var entries = this.selectedEntries_;
    var dragNodes = this.dragNodes_ = [];
    var files = this.files_ = [];

    for (var i = 0; i < entries.length; i++) {
      // File object must be prepeared in advance for clipboard operations
      // (copy, paste and drag). DataTransfer object closes for write after
      // returning control from that handlers so they may not have
      // asynchronous operations.
      if (!this.isOnGData && entries[i].isFile)
        entries[i].file(function(file) { files.push(file); });

      // Items to drag are created in advance. Images must be loaded
      // at the time the 'dragstart' event comes. Otherwise draggable
      // image will be rendered without IMG tags.
      if (dragNodes.length < MAX_DRAG_THUMBAIL_COUNT)
        dragNodes.push(new this.dragNodeConstructor_(entries[i]));
    }

    if (this.isOnGData) {
      this.allGDataFilesAvailable = false;
      var urls = entries.map(function(e) { return e.toURL() });
      this.directoryModel_.getMetadataCache().get(
          urls, 'gdata', function(props) {
        // We consider directories not available offline for the purposes of
        // file transfer since we cannot afford to recursive traversal.
        this.allGDataFilesAvailable =
            entries.filter(function(e) {return e.isDirectory}).length == 0 &&
            props.filter(function(p) {return !p.availableOffline}).length == 0;
        // |Copy| is the only menu item affected by allGDataFilesAvailable.
        // It could be open right now, update its UI.
        this.copyCommand_.disabled = !this.canCopyOrDrag_();
      }.bind(this));
    }
  },

  get currentDirectory() {
    return this.directoryModel_.getCurrentDirEntry();
  },

  get readonly() {
    return this.directoryModel_.isReadOnly();
  },

  get isOnGData() {
    return this.directoryModel_.getRootType() == DirectoryModel.RootType.GDATA;
  },

  notify_: function(eventName) {
    var self = this;
    // Set timeout to avoid recursive events.
    setTimeout(function() {
      cr.dispatchSimpleEvent(self, eventName);
    }, 0);
  },

  /**
   * @type {Array.<Entry>}
   */
  get selectedEntries_() {
    var list = this.fileList_;
    return this.fileListSelection_.selectedIndexes.map(function(index) {
      return list.item(index);
    });
  },

  selectDropEffect_: function(event, destinationPath) {
    if (!destinationPath ||
        this.directoryModel_.isPathReadOnly(destinationPath))
      return 'none';
    if (event.dataTransfer.effectAllowed == 'copyMove' &&
        this.getSourceRoot_(event.dataTransfer) ==
            DirectoryModel.getRootPath(destinationPath) &&
        !event.ctrlKey) {
      return 'move';
    }
    if (event.dataTransfer.effectAllowed == 'copyMove' &&
        event.shiftKey) {
      return 'move';
    }
    return 'copy';
  },

  calculateScrollSpeed_: function(distance) {
    var SCROLL_AREA = 25;  // Pixels.
    var MIN_SCROLL_SPEED = 50; // Pixels/sec.
    var MAX_SCROLL_SPEED = 300;  // Pixels/sec.
    if (distance < 0 || distance > SCROLL_AREA)
      return 0;
    return MAX_SCROLL_SPEED - (MAX_SCROLL_SPEED - MIN_SCROLL_SPEED) *
        (distance / SCROLL_AREA);
  },

  setScrollSpeed_: function(list, speed) {
    var SCROLL_INTERVAL = 200;   // Milliseconds.
    if (speed == 0 && this.scrollInterval_) {
      clearInterval(this.scrollInterval_);
      this.scrollInterval_ = null;
    } else if (speed != 0 && !this.scrollInterval_) {
      this.scrollInterval_ = setInterval(this.scroll_.bind(this),
                                         SCROLL_INTERVAL);
    }
    this.scrollStep_ = speed * SCROLL_INTERVAL / 1000;
    this.scrollList_ = list;
  },

  scroll_: function() {
    if (this.scrollList_)
      this.scrollList_.scrollTop += this.scrollStep_;
  }
};

