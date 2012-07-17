// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This object encapsulates everything related to tasks execution.
 * @param {FileManager} fileManager FileManager instance.
 * @param {Array.<string>} urls List of file urls.
 */
function FileTasks(fileManager, urls) {
  this.fileManager_ = fileManager;
  this.urls_ = urls;
  this.tasks_ = null;
  this.defaultTask_ = null;

  /**
   * List of invocations to be called once tasks are available.
   */
  this.pendingInvocations_ = [];

  if (urls.length > 0)
    chrome.fileBrowserPrivate.getFileTasks(urls, this.onTasks_.bind(this));
}

/**
* Location of the FAQ about the file actions.
*/
FileTasks.NO_ACTION_FOR_FILE_URL = 'http://support.google.com/chromeos/bin/' +
    'answer.py?answer=1700055&topic=29026&ctx=topic';

/**
 * Callback when tasks found.
 * @param {Array.<Object>} tasks The tasks.
 * @private
 */
FileTasks.prototype.onTasks_ = function(tasks) {
  this.processTasks_(tasks);
  for (var index = 0; index < this.pendingInvocations_.length; index++) {
    var name = this.pendingInvocations_[index][0];
    var args = this.pendingInvocations_[index][1];
    this[name].apply(this, args);
  }
  this.pendingInvocations_ = [];
};

/**
 * Processes internal tasks.
 * @param {Array.<Object>} tasks The tasks.
 * @private
 */
FileTasks.prototype.processTasks_ = function(tasks) {
  this.tasks_ = [];
  var id = this.fileManager_.getExtensionId();

  for (var i = 0; i < tasks.length; i++) {
    var task = tasks[i];

    // Tweak images, titles of internal tasks.
    var task_parts = task.taskId.split('|');
    if (task_parts[0] == id) {
      if (task_parts[1] == 'play') {
        // TODO(serya): This hack needed until task.iconUrl is working
        //             (see GetFileTasksFileBrowserFunction::RunImpl).
        task.iconUrl =
            chrome.extension.getURL('images/filetype_audio.png');
        task.title = loadTimeData.getString('ACTION_LISTEN');
      } else if (task_parts[1] == 'mount-archive') {
        task.iconUrl =
            chrome.extension.getURL('images/filetype_archive.png');
        task.title = loadTimeData.getString('MOUNT_ARCHIVE');
      } else if (task_parts[1] == 'gallery') {
        task.title = loadTimeData.getString('ACTION_OPEN');
        task.iconUrl =
            chrome.extension.getURL('images/filetype_image.png');
      } else if (task_parts[1] == 'watch') {
        task.iconUrl =
            chrome.extension.getURL('images/filetype_video.png');
        task.title = loadTimeData.getString('ACTION_WATCH');
      } else if (task_parts[1] == 'open-hosted') {
        if (this.urls_.length > 1) {
          task.iconUrl =
              chrome.extension.getURL('images/filetype_generic.png');
        } else {
          // Use specific icon.
          var icon = FileType.getIcon(this.urls_[0]);
          task.iconUrl =
              chrome.extension.getURL('images/filetype_' + icon + '.png');
        }
        task.title = loadTimeData.getString('ACTION_OPEN');
      } else if (task_parts[1] == 'view-pdf') {
        // Do not render this task if disabled.
        if (!loadTimeData.getBoolean('PDF_VIEW_ENABLED'))
          continue;
        task.iconUrl =
            chrome.extension.getURL('images/filetype_pdf.png');
        task.title = loadTimeData.getString('ACTION_VIEW');
      } else if (task_parts[1] == 'view-in-browser') {
        task.iconUrl =
            chrome.extension.getURL('images/filetype_generic.png');
        task.title = loadTimeData.getString('ACTION_VIEW');
      } else if (task_parts[1] == 'install-crx') {
        task.iconUrl =
            chrome.extension.getURL('images/filetype_generic.png');
        task.title = loadTimeData.getString('INSTALL_CRX');
      }
    }

    this.tasks_.push(task);
    if (this.defaultTask_ == null) {
      this.defaultTask_ = task;
    }
  }
};

/**
 * Executes default task.
 * @private
 */
FileTasks.prototype.executeDefault_ = function() {
  if (this.defaultTask_ != null) {
    this.execute_(this.defaultTask_.taskId);
    return;
  }

  // We don't have tasks, so try to show a file in a browser tab.
  // We only do that for single selection to avoid confusion.
  if (this.urls_.length == 1) {
    var callback = function(success) {
      if (!success) {
        var filename = decodeURIComponent(this.urls_[0]);
        if (filename.indexOf('/') != -1)
          filename = filename.substr(filename.lastIndexOf('/') + 1);

        var text = loadTimeData.getStringF('NO_ACTION_FOR_FILE',
                                           FileTasks.NO_ACTION_FOR_FILE_URL);
        this.fileManager_.alert.showHtml(filename, text, function() {});
      }
    }.bind(this);

    this.checkAvailability_(function() {
      chrome.fileBrowserPrivate.viewFiles(this.urls_, 'default', callback);
    }.bind(this));
  }

  // Do nothing for multiple urls.
};

/**
 * Executes a single task.
 * @param {string} taskId Task identifier.
 * @param {Array.<string>=} opt_urls Urls to execute on instead of |this.urls_|.
 * @private
 */
FileTasks.prototype.execute_ = function(taskId, opt_urls) {
  var urls = opt_urls || this.urls_;
  this.checkAvailability_(function() {
    chrome.fileBrowserPrivate.executeTask(taskId, urls);

    var task_parts = taskId.split('|');
    if (task_parts[0] == this.fileManager_.getExtensionId()) {
      // For internal tasks we do not listen to the event to avoid
      // handling the same task instance from multiple tabs.
      // So, we manually execute the task.
      this.executeInternalTask_(task_parts[1], urls);
    }
  }.bind(this));
};

/**
 * Checks whether the remote files are available right now.
 * @param {function} callback The callback.
 * @private
 */
FileTasks.prototype.checkAvailability_ = function(callback) {
  function areAll(props, name) {
    function isOne(e) {
      // If got no properties, we safely assume that item is unavailable.
      return e && e[name];
    }
    return props.filter(isOne).length == props.length;
  }

  var fm = this.fileManager_;
  var urls = this.urls_;

  if (fm.isOnGData() && fm.isOffline()) {
    fm.metadataCache_.get(urls, 'gdata', function(props) {
      if (areAll(props, 'availableOffline')) {
        callback();
        return;
      }

      fm.alert.showHtml(
          loadTimeData.getString('OFFLINE_HEADER'),
          props[0].hosted ?
            loadTimeData.getStringF(
                urls.length == 1 ?
                    'HOSTED_OFFLINE_MESSAGE' :
                    'HOSTED_OFFLINE_MESSAGE_PLURAL') :
            loadTimeData.getStringF(
                urls.length == 1 ?
                    'OFFLINE_MESSAGE' :
                    'OFFLINE_MESSAGE_PLURAL',
                loadTimeData.getString('OFFLINE_COLUMN_LABEL')));
    });
    return;
  }

  if (fm.isOnGData() && fm.isOnMeteredConnection()) {
    fm.metadataCache_.get(urls, 'gdata', function(gdataProps) {
      if (areAll(gdataProps, 'availableWhenMetered')) {
        callback();
        return;
      }

      fm.metadataCache_.get(urls, 'filesystem', function(fileProps) {
        var sizeToDownload = 0;
        for (var i = 0; i != urls.length; i++) {
          if (!gdataProps[i].availableWhenMetered)
            sizeToDownload += fileProps[i].size;
        }
        fm.confirm.show(
            loadTimeData.getStringF(
                urls.length == 1 ?
                    'CONFIRM_MOBILE_DATA_USE' :
                    'CONFIRM_MOBILE_DATA_USE_PLURAL',
                util.bytesToSi(sizeToDownload)),
            callback);
      });
    });
    return;
  }

  callback();
};

/**
 * Executes an internal task.
 * @param {string} id The short task id.
 * @param {Array.<string>} urls The urls to execute on.
 * @private
 */
FileTasks.prototype.executeInternalTask_ = function(id, urls) {
  var fm = this.fileManager_;

  if (id == 'play') {
    var position = 0;
    if (urls.length == 1) {
      // If just a single audio file is selected pass along every audio file
      // in the directory.
      var selectedUrl = urls[0];
      urls = fm.getAllUrlsInCurrentDirectory().filter(FileType.isAudio);
      position = urls.indexOf(selectedUrl);
    }
    chrome.mediaPlayerPrivate.play(urls, position);
    return;
  }

  if (id == 'mount-archive') {
    this.mountArchives_(urls);
    return;
  }

  if (id == 'format-device') {
    fm.confirm.show(loadTimeData.getString('FORMATTING_WARNING'), function() {
      chrome.fileBrowserPrivate.formatDevice(urls[0]);
    });
    return;
  }

  if (id == 'gallery') {
    this.openGallery_(urls);
    return;
  }

  if (id == 'view-pdf' || id == 'view-in-browser' ||
      id == 'install-crx' || id == 'open-hosted' || id == 'watch') {
    chrome.fileBrowserPrivate.viewFiles(urls, id, function(success) {
      if (!success)
        console.error('chrome.fileBrowserPrivate.viewFiles failed', urls);
    });
    return;
  }
};

/**
 * Mounts archives.
 * @param {Array.<string>} urls Mount file urls list.
 * @private
 */
FileTasks.prototype.mountArchives_ = function(urls) {
  var fm = this.fileManager_;

  var tracker = fm.directoryModel_.createDirectoryChangeTracker();
  tracker.start();

  fm.resolveSelectResults_(urls, function(urls) {
    for (var index = 0; index < urls.length; ++index) {
      // TODO(kaznacheev): Incapsulate URL to path conversion.
      var path =
          /^filesystem:[\w-]*:\/\/[\w]*\/(external|persistent)(\/.*)$/.
              exec(urls[index])[2];
      if (!path)
        continue;
      path = decodeURIComponent(path);

      fm.volumeManager_.mountArchive(path, function(mountPath) {
        console.log('Mounted at: ', mountPath);
        tracker.stop();
        if (!tracker.hasChanged)
          fm.directoryModel_.changeDirectory(mountPath);
      }, function(error) {
        tracker.stop();
        var namePos = path.lastIndexOf('/');
        fm.alert.show(strf('ARCHIVE_MOUNT_FAILED',
                           path.substr(namePos + 1), error));
      });
    }
  });
};

/**
 * Opens provided urls in the gallery.
 * @param {Array.<string>} urls List of all the urls that will be shown in
 *     the gallery.
 * @private
 */
FileTasks.prototype.openGallery_ = function(urls) {
  var fm = this.fileManager_;
  var singleSelection = urls.length == 1;

  var selectedUrl;
  if (singleSelection && FileType.isImage(urls[0])) {
    // Single image item selected. Pass to the Gallery as a selected.
    selectedUrl = urls[0];
    // Pass along every image and video in the directory so that it shows up
    // in the ribbon.
    // We do not do that if a single video is selected because the UI is
    // cleaner without the ribbon.
    urls = fm.getAllUrlsInCurrentDirectory().filter(FileType.isImageOrVideo);
  } else {
    // Pass just the selected items, select the first entry.
    selectedUrl = urls[0];
  }

  var galleryFrame = fm.document_.createElement('iframe');
  galleryFrame.className = 'overlay-pane';
  galleryFrame.scrolling = 'no';
  galleryFrame.setAttribute('webkitallowfullscreen', true);

  var dirPath = fm.getCurrentDirectory();
  // Push a temporary state which will be replaced every time an individual
  // item is selected in the Gallery.
  fm.updateLocation_(false /*push*/, dirPath);

  var getShareActions = function(urls, callback) {
    this.getExternals_(callback);
  }.bind(this);

  galleryFrame.onload = function() {
    fm.show_();
    galleryFrame.contentWindow.ImageUtil.metrics = metrics;
    galleryFrame.contentWindow.FileType = FileType;
    galleryFrame.contentWindow.util = util;

    var readonly = fm.isOnReadonlyDirectory();
    var currentDir = fm.directoryModel_.getCurrentDirEntry();
    var downloadsDir = fm.directoryModel_.getRootsList().item(0);
    var readonlyDirName = null;
    if (readonly) {
      readonlyDirName = fm.isOnGData() ?
          fm.getRootLabel_(PathUtil.getRootPath(currentDir.fullPath)) :
          fm.directoryModel_.getCurrentRootName();
    }

    var gallerySelection;
    var context = {
      // We show the root label in readonly warning (e.g. archive name).
      readonlyDirName: readonlyDirName,
      saveDirEntry: readonly ? downloadsDir : currentDir,
      metadataCache: fm.metadataCache_,
      getShareActions: getShareActions,
      onNameChange: function(name) {
        fm.document_.title = gallerySelection = name;
        fm.updateLocation_(true /*replace*/, dirPath + '/' + name);
      },
      onClose: function() {
        if (singleSelection)
          fm.directoryModel_.selectEntry(gallerySelection);
        history.back(1);
      },
      displayStringFunction: loadTimeData.getStringF.bind(loadTimeData)
    };
    galleryFrame.contentWindow.Gallery.open(context, urls, selectedUrl);
  };

  galleryFrame.src = 'gallery.html';
  fm.openFilePopup_(galleryFrame, fm.updateTitle_.bind(this));
};

/**
 * Displays the list of tasks in a task picker combobutton.
 * @param {cr.ui.ComboButton} combobutton The task picker element.
 * @private
 */
FileTasks.prototype.display_ = function(combobutton) {
  if (this.tasks_.length == 0) {
    combobutton.hidden = true;
    return;
  }

  combobutton.hidden = false;
  combobutton.defaultItem = this.createCombobuttonItem_(this.defaultTask_);

  var items = [];
  var title = this.defaultTask_.title + ' ' +
              loadTimeData.getString('DEFAULT_ACTION_LABEL');
  items.push(this.createCombobuttonItem_(this.defaultTask_, title));

  for (var index = 0; index < this.tasks_.length; index++) {
    var task = this.tasks_[index];
    if (task != this.defaultTask_)
      items.push(this.createCombobuttonItem_(task));
  }

  if (items.length > 1) {
    items.sort(function(a, b) {
      return a.label.localeCompare(b.label);
    });

    var defaultIdx = 0;
    for (var j = 0; j < items.length; j++) {
      combobutton.addDropDownItem(items[j]);
      if (items[j].task.taskId == this.defaultTask_.taskId)
        defaultIdx = j;
    }

    combobutton.addSeparator();
    combobutton.addDropDownItem({
          label: loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM'),
          items: items,
          defaultIdx: defaultIdx
        });
  }
};

/**
 * Returns a list of external tasks (i.e. not defined in file manager).
 * @param {function(Array.<Object>)} callback The callback.
 * @private
 */
FileTasks.prototype.getExternals_ = function(callback) {
  var externals = [];
  var id = this.fileManager_.getExtensionId();
  for (var index = 0; index < this.tasks.length; index++) {
    var task = this.tasks[index];
    var task_parts = task.taskId.split('|');
    if (task_parts[0] != id) {
      // Add callback, so gallery can execute the task.
      task.execute = this.execute_.bind(this, task.taskId);
      externals.push(task);
    }
  }
  callback(externals);
};

/**
 * Creates combobutton item based on task.
 * @param {Object} task Task to convert.
 * @param {string=} opt_title Title.
 * @return {Object} Item appendable to combobutton drop-down list.
 * @private
 */
FileTasks.prototype.createCombobuttonItem_ = function(task, opt_title) {
  return { label: opt_title || task.title, iconUrl: task.iconUrl, task: task };
};


/**
 * Decorates a FileTasks method, so it will be actually executed after the tasks
 * are available.
 * This decorator expects an implementation called |method + '_'|.
 * @param {string} method The method name.
 */
FileTasks.decorate = function(method) {
  var privateMethod = method + '_';
  FileTasks.prototype[method] = function() {
    if (this.tasks_) {
      this[privateMethod].apply(this, arguments);
    } else {
      this.pendingInvocations_.push([privateMethod, arguments]);
    }
    return this;
  };
};

FileTasks.decorate('display');
FileTasks.decorate('execute');
FileTasks.decorate('executeDefault');
FileTasks.decorate('getExternals');
