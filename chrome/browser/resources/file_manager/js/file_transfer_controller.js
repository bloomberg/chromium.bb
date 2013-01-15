// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Global (placed in the window object) variable name to hold internal
 * file dragging information. Needed to show visual feedback while dragging
 * since DataTransfer object is in protected state. Reachable from other
 * file manager instances.
 */
var DRAG_AND_DROP_GLOBAL_DATA = '__drag_and_drop_global_data';

/**
 * @constructor
 * @param {HTMLDocument} doc Owning document.
 * @param {FileCopyManager} copyManager Copy manager instance.
 * @param {DirectoryModel} directoryModel Directory model instance.
 */
function FileTransferController(doc,
                                copyManager,
                                directoryModel) {
  this.document_ = doc;
  this.copyManager_ = copyManager;
  this.directoryModel_ = directoryModel;

  this.directoryModel_.getFileListSelection().addEventListener('change',
      this.onSelectionChanged_.bind(this));

  /**
   * DOM element to represent selected file in drag operation. Used if only
   * one element is selected.
   * @type {HTMLElement}
   * @private
   */
  this.preloadedThumbnailImageNode_ = null;

  /**
   * File objects for seletced files.
   * @type {Array.<File>}
   * @private
   */
  this.selectedFileObjects_ = [];
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

  attachBreadcrumbsDropTarget: function(breadcrumbsController) {
    var container = breadcrumbsController.getContainer();
    container.addEventListener('dragover',
        this.onDragOver_.bind(this, true, null));
    container.addEventListener('dragenter',
        this.onDragEnterBreadcrumbs_.bind(this, breadcrumbsController));
    container.addEventListener('dragleave',
        this.onDragLeave_.bind(this, null));
    container.addEventListener('drop', this.onDrop_.bind(this, true));
  },

  /**
   * Attach handlers of copy, cut and paste operations to the document.
   */
  attachCopyPasteHandlers: function() {
    this.document_.addEventListener('beforecopy',
                                    this.onBeforeCopy_.bind(this));
    this.document_.addEventListener('copy',
                                    this.onCopy_.bind(this));
    this.document_.addEventListener('beforecut',
                                    this.onBeforeCut_.bind(this));
    this.document_.addEventListener('cut',
                                    this.onCut_.bind(this));
    this.document_.addEventListener('beforepaste',
                                    this.onBeforePaste_.bind(this));
    this.document_.addEventListener('paste',
                                    this.onPaste_.bind(this));
    this.copyCommand_ = this.document_.querySelector('command#copy');
  },

  /**
   * Write the current selection to system clipboard.
   *
   * @param {DataTransfer} dataTransfer DataTransfer from the event.
   * @param {string} effectAllowed Value must be valid for the
   *     |dataTransfer.effectAllowed| property ('move', 'copy', 'copyMove').
   */
  cutOrCopy_: function(dataTransfer, effectAllowed) {
    var directories = [];
    var files = [];
    var entries = this.selectedEntries_;
    for (var i = 0; i < entries.length; i++) {
      (entries[i].isDirectory ? directories : files).push(entries[i].fullPath);
    }

    // Tag to check it's filemanager data.
    dataTransfer.setData('fs/tag', 'filemanager-data');

    dataTransfer.setData('fs/isOnGData', this.isOnGData);
    if (this.currentDirectory)
      dataTransfer.setData('fs/sourceDir', this.currentDirectory.fullPath);
    dataTransfer.setData('fs/directories', directories.join('\n'));
    dataTransfer.setData('fs/files', files.join('\n'));
    dataTransfer.effectAllowed = effectAllowed;
    dataTransfer.setData('fs/effectallowed', effectAllowed);

    for (var i = 0; i < this.selectedFileObjects_.length; i++) {
      dataTransfer.items.add(this.selectedFileObjects_[i]);
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
      return PathUtil.getRootPath(sourceDir);

    // For drive search, sourceDir will be set to null, so we should double
    // check that we are not on drive.
    if (dataTransfer.getData('fs/isOnGData') == 'true')
      return '/' + DirectoryModel.GDATA_DIRECTORY;

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
   * @return {string} Either "copy" or "move".
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
      var targetOnGData = (PathUtil.getRootType(destinationPath) ===
                           RootType.GDATA);
      this.copyManager_.paste(operationInfo,
                              destinationPath,
                              targetOnGData);
    } else {
      console.log('Ignore move into the same folder');
    }

    return toMove ? 'move' : 'copy';
  },

  /**
   * Preloads an image thumbnail for the specified file entry.
   * @param {Entry} entry Entry to preload a thumbnail for.
   */
  preloadThumbnailImage_: function(entry) {
    var imageUrl = entry.toURL();
    var metadataTypes = 'thumbnail|filesystem';
    this.preloadedThumbnailImageNode_ = this.document_.createElement('div');
    this.preloadedThumbnailImageNode_.className = 'img-container';
    this.directoryModel_.getMetadataCache().get(
        imageUrl,
        metadataTypes,
        function(metadata) {
          new ThumbnailLoader(imageUrl, metadata).
              load(this.preloadedThumbnailImageNode_, true);
        }.bind(this));
  },

  /**
   * Renders a drag-and-drop thumbnail.
   * @return {HTMLElement} Element containing the thumbnail.
   */
  renderThumbnail_: function() {
    var length = this.selectedEntries_.length;

    var container = this.document_.querySelector('#drag-container');
    var contents = this.document_.createElement('div');
    contents.className = 'drag-contents';
    container.appendChild(contents);

    var thumbnailImage;
    if (this.preloadedThumbnailImageNode_)
      thumbnailImage = this.preloadedThumbnailImageNode_.querySelector('img');

    // Option 1. Multiple selection, render only a label.
    if (length > 1) {
      var label = this.document_.createElement('div');
      label.className = 'label';
      label.textContent = strf('DRAGGING_MULTIPLE_ITEMS', length);
      contents.appendChild(label);
      return container;
    }

    // Option 2. Thumbnail image available, then render it without
    // a label.
    if (thumbnailImage) {
      contents.classList.add('drag-image-thumbnail');
      contents.appendChild(this.preloadedThumbnailImageNode_);
      return container;
    }

    // Option 3. Thumbnail not available. Render an icon and a label.
    var entry = this.selectedEntries_[0];
    var icon = this.document_.createElement('div');
    icon.className = 'detail-icon';
    icon.setAttribute('file-type-icon', FileType.getIcon(entry));
    contents.appendChild(icon);
    var label = this.document_.createElement('div');
    label.className = 'label';
    label.textContent = entry.name;
    contents.appendChild(label);
    return container;
  },

  onDragStart_: function(list, event) {
    var dt = event.dataTransfer;
    var dragThumbnail = this.renderThumbnail_();
    dt.setDragImage(dragThumbnail, 1000, 1000);

    if (this.canCopyOrDrag_(dt)) {
      if (this.canCutOrDrag_(dt))
        this.cutOrCopy_(dt, 'copyMove');
      else
        this.cutOrCopy_(dt, 'copy');
    } else {
      event.preventDefault();
    }

    window[DRAG_AND_DROP_GLOBAL_DATA] = {
      sourceRoot: this.directoryModel_.getCurrentRootPath()
    };
  },

  onDragEnd_: function(list, event) {
    var container = this.document_.querySelector('#drag-container');
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
    event.preventDefault();  // Required to prevent the cursor flicker.
    this.lastEnteredTarget_ = event.target;
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

  onDragEnterBreadcrumbs_: function(breadcrumbsContainer, event) {
    event.preventDefault();  // Required to prevent the cursor flicker.
    this.lastEnteredTarget_ = event.target;
    var path = breadcrumbsContainer.getTargetPath(event);
    if (!path)
      return;

    this.setDropTarget_(event.target, true, event.dataTransfer, path);
  },

  onDragLeave_: function(list, event) {
    // If mouse moves from one element to another the 'dragenter'
    // event for the new element comes before the 'dragleave' event for
    // the old one. In this case event.target != this.lastEnteredTarget_
    // and handler of the 'dragenter' event has already caried of
    // drop target. So event.target == this.lastEnteredTarget_
    // could only be if mouse goes out of listened element.
    if (event.target == this.lastEnteredTarget_) {
      this.setDropTarget_(null);
      this.lastEnteredTarget_ = null;
    }
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
        this.directoryModel_.changeDirectory(opt_destinationPath);
      }.bind(this), 2000);
    }
  },

  isDocumentWideEvent_: function(event) {
    return this.document_.activeElement.nodeName.toLowerCase() != 'input' ||
        this.document_.activeElement.type.toLowerCase() != 'text';
  },

  onCopy_: function(event) {
    if (!this.isDocumentWideEvent_(event) ||
        !this.canCopyOrDrag_()) {
      return;
    }
    event.preventDefault();
    this.cutOrCopy_(event.clipboardData, 'copy');
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
    this.cutOrCopy_(event.clipboardData, 'move');
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
    if (this.directoryModel_.isSearching())
      return false;

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
    if (!this.isDocumentWideEvent_()) {
      return false;
    }

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
    var files = this.selectedFileObjects_ = [];
    this.preloadedThumbnailImageNode_ = null;

    var fileEntries = [];
    for (var i = 0; i < entries.length; i++) {
      if (entries[i].isFile)
        fileEntries.push(entries[i]);
    }

    if (entries.length == 1) {
      // For single selection, the dragged element is created in advance,
      // otherwise an image may not be loaded at the time the 'dragstart' event
      // comes.
      this.preloadThumbnailImage_(entries[0]);
    }

    // File object must be prepeared in advance for clipboard operations
    // (copy, paste and drag). DataTransfer object closes for write after
    // returning control from that handlers so they may not have
    // asynchronous operations.
    function prepareFileObjects() {
      for (var i = 0; i < fileEntries.length; i++) {
        fileEntries[i].file(function(file) { files.push(file); });
      }
    };

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

        if (this.allGDataFilesAvailable)
          prepareFileObjects();
      }.bind(this));
    } else {
      prepareFileObjects();
    }
  },

  get currentDirectory() {
    if (this.directoryModel_.isSearching() && this.isOnGData)
      return null;
    return this.directoryModel_.getCurrentDirEntry();
  },

  get readonly() {
    return this.directoryModel_.isReadOnly();
  },

  get isOnGData() {
    return this.directoryModel_.getCurrentRootType() === RootType.GDATA;
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
    var list = this.directoryModel_.getFileList();
    var selectedIndexes = this.directoryModel_.getFileListSelection().
                               selectedIndexes;
    var entries = selectedIndexes.map(function(index) {
      return list.item(index);
    });

    // TODO(serya): Diagnostics for http://crbug/129642
    if (entries.indexOf(undefined) != -1) {
      var index = entries.indexOf(undefined);
      entries = entries.filter(function(e) { return !!e; });
      console.error('Invalid selection found: list items: ', list.length,
                    'wrong indexe value: ', selectedIndexes[index],
                    'Stack trace: ', new Error().stack);
    }
    return entries;
  },

  selectDropEffect_: function(event, destinationPath) {
    if (!destinationPath ||
        this.directoryModel_.isPathReadOnly(destinationPath))
      return 'none';
    if (event.dataTransfer.effectAllowed == 'copyMove' &&
        this.getSourceRoot_(event.dataTransfer) ==
            PathUtil.getRootPath(destinationPath) &&
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

