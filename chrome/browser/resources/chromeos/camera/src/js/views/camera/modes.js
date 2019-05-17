// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Namespace for Camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * Mode controller managing capture sequence of different camera mode.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @param {function()} doSwitchResolution Callback to trigger resolution
 *     switching.
 * @param {function()} doSwitchMode Callback to trigger mode switching.
 * @param {function(?Blob, boolean, string): Promise} doSavePicture Callback for
 *     saving picture.
 * @constructor
 */
cca.views.camera.Modes = function(
    resolBroker, doSwitchResolution, doSwitchMode, doSavePicture) {
  /**
   * @type {function()}
   * @private
   */
  this.doSwitchMode_ = doSwitchMode;

  /**
   * Callback for saving picture.
   * @type {function(?Blob, boolean, string): Promise}
   * @private
   */
  this.doSavePicture_ = doSavePicture;

  /**
   * Capture controller of current camera mode.
   * @type {cca.views.camera.Mode}
   */
  this.current = null;

  /**
   * Stream of current mode.
   * @type {MediaStream}
   * @private
   */
  this.stream_ = null;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.modesGroup_ = document.querySelector('#modes-group');

  /**
   * @type {cca.views.camera.PhotoResolutionConfig}
   * @private
   */
  this.photoResConfig_ = new cca.views.camera.PhotoResolutionConfig(
      resolBroker, doSwitchResolution);

  /**
   * @type {cca.views.camera.VideoResolutionConfig}
   * @private
   */
  this.videoResConfig_ = new cca.views.camera.VideoResolutionConfig(
      resolBroker, doSwitchResolution);

  /**
   * Captured resolution width.
   * @type {number}
   * @private
   */
  this.captureWidth_ = 0;

  /**
   * Captured resolution height.
   * @type {number}
   * @private
   */
  this.captureHeight_ = 0;

  /**
   * Mode classname and related functions and attributes.
   * @type {Object<string, Object>}
   * @private
   */
  this.allModes_ = {
    'video-mode': {
      captureFactory: () =>
          new cca.views.camera.Video(this.stream_, this.doSavePicture_),
      isSupported: async () => true,
      resolutionConfig: this.videoResConfig_,
      nextMode: 'photo-mode',
    },
    'photo-mode': {
      captureFactory: () => new cca.views.camera.Photo(
          this.stream_, this.doSavePicture_, this.captureWidth_,
          this.captureHeight_),
      isSupported: async () => true,
      resolutionConfig: this.photoResConfig_,
      nextMode: 'square-mode',
    },
    'square-mode': {
      captureFactory: () => new cca.views.camera.Square(
          this.stream_, this.doSavePicture_, this.captureWidth_,
          this.captureHeight_),
      isSupported: async () => true,
      resolutionConfig: this.photoResConfig_,
      nextMode: 'portrait-mode',
    },
    'portrait-mode': {
      captureFactory: () => new cca.views.camera.Portrait(
          this.stream_, this.doSavePicture_, this.captureWidth_,
          this.captureHeight_),
      isSupported: async (stream) => {
        try {
          const imageCapture =
              new cca.mojo.ImageCapture(stream.getVideoTracks()[0]);
          const capabilities = await imageCapture.getPhotoCapabilities();
          return capabilities.supportedEffects &&
              capabilities.supportedEffects.includes(
                  cros.mojom.Effect.PORTRAIT_MODE);
        } catch (e) {
          // The mode is considered unsupported for given stream. This includes
          // the case where underlying camera HAL is V1 causing mojo connection
          // unable to work.
          return false;
        }
      },
      resolutionConfig: this.photoResConfig_,
      nextMode: 'video-mode',
    },
  };

  // End of properties, seal the object.
  Object.seal(this);
  document.querySelectorAll('.mode-item>input').forEach((element) => {
    element.addEventListener('click', (event) => {
      if (!cca.state.get('streaming') || cca.state.get('taking')) {
        event.preventDefault();
      }
    });
    element.addEventListener('change', (event) => {
      if (element.checked) {
        var mode = element.dataset.mode;
        this.updateModeUI_(mode);
        cca.state.set('mode-switching', true);
        this.doSwitchMode_().then(() => cca.state.set('mode-switching', false));
      }
    });
  });

  // Set default mode when app started.
  this.updateModeUI_('photo-mode');
};

/**
 * Updates state of mode related UI to the target mode.
 * @param {string} mode Mode to be toggled.
 */
cca.views.camera.Modes.prototype.updateModeUI_ = function(mode) {
  Object.keys(this.allModes_).forEach((m) => cca.state.set(m, m == mode));
  const element = document.querySelector(`.mode-item>input[data-mode=${mode}]`);
  element.checked = true;
  const wrapper = element.parentElement;
  this.modesGroup_.scrollTo({
    left: 0,
    top: wrapper.offsetTop - this.modesGroup_.offsetHeight / 2 +
        wrapper.offsetHeight / 2,
    behavior: 'smooth',
  });
};

/**
 * Controller for generating suitable capture and preview resolution
 * combination.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @param {function()} doSwitchResolution
 * @constructor
 */
cca.views.camera.ModeResolutionConfig = function(
    resolBroker, doSwitchResolution) {
  /**
   * @type {cca.ResolutionEventBroker}
   * @private
   */
  this.resolBroker_ = resolBroker;

  /**
   * @type {function()}
   * @private
   */
  this.doSwitchResolution_ = doSwitchResolution;

  /**
   * Object saving resolution preference for each of its key as device id and
   * value to be preferred width, height of resolution of that video device.
   * @type {Object<string, {width: number, height: number}>}
   * @private
   */
  this.prefResolution_ = null;

  /**
   * Device id of currently working video device.
   * @type {string}
   * @private
   */
  this.deviceId_ = null;

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Gets all available resolutions candidates for capturing under this controller
 * and its corresponding preview constraints from all available resolutions of
 * different format. Returned resolutions and constraints candidates are both
 * sorted in desired trying order.
 * @param {string} deviceId
 * @param {ResolList} photoResolutions
 * @param {ResolList} videoResolutions
 * @return {Array<[number, number, Array<Object>]>} Result capture resolution
 *     width, height and constraints-candidates for its preview.
 */
cca.views.camera.ModeResolutionConfig.prototype.getSortedCandidates = function(
    deviceId, photoResolutions, videoResolutions) {
  return null;
};

/**
 * Updates selected resolution.
 * @param {string} deviceId Device id of selected video device.
 * @param {number} width Width of selected resolution.
 * @param {number} height Height of selected resolution.
 */
cca.views.camera.ModeResolutionConfig.prototype.updateSelectedResolution =
    function(deviceId, width, height) {};

/**
 * Controller for generating suitable video recording and preview resolution
 * combination.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @param {function()} doSwitchResolution
 * @constructor
 */
cca.views.camera.VideoResolutionConfig = function(
    resolBroker, doSwitchResolution) {
  cca.views.camera.ModeResolutionConfig.call(
      this, resolBroker, doSwitchResolution);

  // Restore saved preferred video resolution per video device.
  chrome.storage.local.get(
      {deviceVideoResolution: {}},
      (values) => this.prefResolution_ = values.deviceVideoResolution);

  this.resolBroker_.registerChangeVideoResolHandler(
      (deviceId, width, height) => {
        this.prefResolution_[deviceId] = {width, height};
        chrome.storage.local.set({deviceVideoResolution: this.prefResolution_});
        if (cca.state.get('video-mode') && deviceId == this.deviceId_) {
          this.doSwitchResolution_();
        }
      });
};

cca.views.camera.VideoResolutionConfig.prototype = {
  __proto__: cca.views.camera.ModeResolutionConfig.prototype,
};

/**
 * @param {string} deviceId
 * @param {number} width
 * @param {number} height
 * @override
 */
cca.views.camera.VideoResolutionConfig.prototype.updateSelectedResolution =
    function(deviceId, width, height) {
  this.deviceId_ = deviceId;
  this.prefResolution_[deviceId] = {width, height};
  chrome.storage.local.set({deviceVideoResolution: this.prefResolution_});
  this.resolBroker_.notifyVideoResolChange(deviceId, width, height);
};

/**
 * @param {string} deviceId
 * @param {ResolList} photoResolutions
 * @param {ResolList} videoResolutions
 * @return {Array<[number, number, Array<Object>]>}
 * @override
 */
cca.views.camera.VideoResolutionConfig.prototype.getSortedCandidates = function(
    deviceId, photoResolutions, videoResolutions) {
  // Due to the limitation of MediaStream API, preview stream is used directly
  // to do video recording.
  const prefR = this.prefResolution_[deviceId] || {width: 0, height: -1};
  const preferredOrder = ([w, h], [w2, h2]) => {
    if (w == w2 && h == h2) {
      return 0;
    }
    // Exactly the preferred resolution.
    if (w == prefR.width && h == prefR.height) {
      return -1;
    }
    if (w2 == prefR.width && h2 == prefR.height) {
      return 1;
    }
    // Aspect ratio same as preferred resolution.
    if (w * h2 != w2 * h) {
      if (w * prefR.height == prefR.width * h) {
        return -1;
      }
      if (w2 * prefR.height == prefR.width * h2) {
        return 1;
      }
    }
    return w2 * h2 - w * h;
  };
  return videoResolutions.sort(preferredOrder).map(([width, height]) => ([
                                                     [width, height],
                                                     [{
                                                       audio: true,
                                                       video: {
                                                         deviceId:
                                                             {exact: deviceId},
                                                         frameRate: {min: 24},
                                                         width,
                                                         height,
                                                       },
                                                     }],
                                                   ]));
};

/**
 * Controller for generating suitable photo taking and preview resolution
 * combination.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @param {function()} doSwitchResolution
 * @constructor
 */
cca.views.camera.PhotoResolutionConfig = function(
    resolBroker, doSwitchResolution) {
  cca.views.camera.ModeResolutionConfig.call(
      this, resolBroker, doSwitchResolution);

  // Restore saved preferred photo resolution per video device.
  chrome.storage.local.get(
      {devicePhotoResolution: {}},
      (values) => this.prefResolution_ = values.devicePhotoResolution);

  this.resolBroker_.registerChangePhotoResolHandler(
      (deviceId, width, height) => {
        this.prefResolution_[deviceId] = {width, height};
        chrome.storage.local.set({devicePhotoResolution: this.prefResolution_});
        if (!cca.state.get('video-mode') && deviceId == this.deviceId_) {
          this.doSwitchResolution_();
        }
      });
};

/**
 * @param {string} deviceId
 * @param {number} width
 * @param {number} height
 * @override
 */
cca.views.camera.PhotoResolutionConfig.prototype.updateSelectedResolution =
    function(deviceId, width, height) {
  this.deviceId_ = deviceId;
  this.prefResolution_[deviceId] = {width, height};
  chrome.storage.local.set({devicePhotoResolution: this.prefResolution_});
  this.resolBroker_.notifyPhotoResolChange(deviceId, width, height);
};

/**
 * Finds and pairs photo resolutions and preview resolutions with the same
 * aspect ratio.
 * @param {ResolList} captureResolutions Available photo capturing resolutions.
 * @param {ResolList} previewResolutions Available preview resolutions.
 * @return {Array<[ResolList, ResolList]>} Each item of returned array is a pair
 *     of capture and preview resolutions of same aspect ratio.
 */
cca.views.camera.PhotoResolutionConfig.prototype
    .pairCapturePreviewResolutions_ = function(
    captureResolutions, previewResolutions) {
  const toAspectRatio = (w, h) => (w / h).toFixed(4);
  const previewRatios = previewResolutions.reduce((rs, [w, h]) => {
    const key = toAspectRatio(w, h);
    rs[key] = rs[key] || [];
    rs[key].push([w, h]);
    return rs;
  }, {});
  const captureRatios = captureResolutions.reduce((rs, [w, h]) => {
    const key = toAspectRatio(w, h);
    if (key in previewRatios) {
      rs[key] = rs[key] || [];
      rs[key].push([w, h]);
    }
    return rs;
  }, {});
  return Object.entries(captureRatios)
      .map(([aspectRatio,
             captureRs]) => [captureRs, previewRatios[aspectRatio]]);
};

/**
 * @param {string} deviceId
 * @param {ResolList} photoResolutions
 * @param {ResolList} videoResolutions
 * @return {Array<[number, number, Array<Object>]>}
 * @override
 */
cca.views.camera.PhotoResolutionConfig.prototype.getSortedCandidates = function(
    deviceId, photoResolutions, videoResolutions) {
  const prefR = this.prefResolution_[deviceId] || {width: 0, height: -1};
  return this.pairCapturePreviewResolutions_(photoResolutions, videoResolutions)
      .map(([captureRs, previewRs]) => {
        if (captureRs.some(([w, h]) => w == prefR.width && h == prefR.height)) {
          var captureR = [prefR.width, prefR.height];
        } else {
          var captureR = captureRs.reduce(
              (captureR, r) => (r[0] > captureR[0] ? r : captureR), [0, -1]);
        }

        const candidates = previewRs.sort(([w, h], [w2, h2]) => w2 - w)
                               .map(([width, height]) => ({
                                      audio: false,
                                      video: {
                                        deviceId: {exact: deviceId},
                                        frameRate: {min: 24},
                                        width,
                                        height,
                                      },
                                    }));
        // Format of map result:
        // [
        //   [[CaptureW 1, CaptureH 1], [CaptureW 2, CaptureH 2], ...],
        //   [PreviewConstraint 1, PreviewConstraint 2, ...]
        // ]
        return [captureR, candidates];
      })
      .sort(([[w, h]], [[w2, h2]]) => {
        if (w == w2 && h == h2) {
          return 0;
        }
        if (w == prefR.width && h == prefR.height) {
          return -1;
        }
        if (w2 == prefR.width && h2 == prefR.height) {
          return 1;
        }
        return w2 * h2 - w * h;
      });
};

/**
 * Switches mode to either video-recording or photo-taking.
 * @param {string} mode Class name of the switching mode.
 * @private
 */
cca.views.camera.Modes.prototype.switchMode_ = function(mode) {
  if (!cca.state.get('streaming') || cca.state.get('taking')) {
    return;
  }
  this.updateModeUI_(mode);
  cca.state.set('mode-switching', true);
  this.doSwitchMode_().then(() => cca.state.set('mode-switching', false));
};

/**
 * Gets all mode candidates. Desired trying sequence of candidate modes is
 * reflected in the order of the returned array.
 * @return {Array<string>} Mode candidates to be tried out.
 */
cca.views.camera.Modes.prototype.getModeCandidates = function() {
  const tried = {};
  const results = [];
  let mode = Object.keys(this.allModes_).find(cca.state.get);
  while (!tried[mode]) {
    tried[mode] = true;
    results.push(mode);
    mode = this.allModes_[mode].nextMode;
  }
  return results;
};

/**
 * Gets all available capture resolution and its corresponding preview
 * constraints for the given mode.
 * @param {string} mode
 * @param {string} deviceId
 * @param {ResolList} photoResolutions
 * @param {ResolList} videoResolutions
 * @return {Array<[number, number, Array<Object>]>} Result capture resolution
 *     width, height and constraints-candidates for its preview.
 */
cca.views.camera.Modes.prototype.getResolutionCandidates = function(
    mode, deviceId, photoResolutions, videoResolutions) {
  return this.allModes_[mode].resolutionConfig.getSortedCandidates(
      deviceId, photoResolutions, videoResolutions);
};

/**
 * Gets supported modes for video device of the given stream.
 * @param {MediaStream} stream Stream of the video device.
 * @return {Array<string>} Names of all supported mode for the video device.
 */
cca.views.camera.Modes.prototype.getSupportedModes = async function(stream) {
  let supportedModes = [];
  for (const [mode, obj] of Object.entries(this.allModes_)) {
    if (await obj.isSupported(stream)) {
      supportedModes.push(mode);
    }
  }
  return supportedModes;
};

/**
 * Updates mode selection UI according to given supported modes.
 * @param {Array<string>} supportedModes Supported mode names to be updated
 *     with.
 */
cca.views.camera.Modes.prototype.updateModeSelectionUI = function(
    supportedModes) {
  document.querySelectorAll('.mode-item').forEach((element) => {
    const radio = element.querySelector('input[type=radio]');
    element.style.display =
        supportedModes.includes(radio.dataset.mode) ? '' : 'none';
  });
};

/**
 * Creates and updates new current mode object.
 * @async
 * @param {string} mode Classname of mode to be updated.
 * @param {MediaStream} stream Stream of the new switching mode.
 * @param {string} deviceId Device id of currently working video device.
 * @param {number} captureWidth Capturing resolution width.
 * @param {number} captureHeight Capturing resolution height.
 */
cca.views.camera.Modes.prototype.updateMode =
    async function(mode, stream, deviceId, captureWidth, captureHeight) {
  if (this.current != null) {
    await this.current.stopCapture();
  }
  this.updateModeUI_(mode);
  this.stream_ = stream;
  this.captureWidth_ = captureWidth;
  this.captureHeight_ = captureHeight;
  this.current = this.allModes_[mode].captureFactory();
  this.allModes_[mode].resolutionConfig.updateSelectedResolution(
      deviceId, captureWidth, captureHeight);
};

/**
 * Base class for controlling capture sequence in different camera modes.
 * @param {MediaStream} stream
 * @param {function(?Blob, boolean, string): Promise} doSavePicture
 * @param {number} captureWidth Capturing resolution width.
 * @param {number} captureHeight Capturing resolution height.
 * @constructor
 */
cca.views.camera.Mode = function(
    stream, doSavePicture, captureWidth, captureHeight) {
  /**
   * Stream of current mode.
   * @type {?Promise}
   * @protected
   */
  this.stream_ = stream;

  /**
   * Callback for saving picture.
   * @type {function(?Blob, boolean, string): Promise}
   * @protected
   */
  this.doSavePicture_ = doSavePicture;

  /**
   * @type {number}
   * @private
   */
  this.captureWidth_ = captureWidth;

  /**
   * @type {number}
   * @private
   */
  this.captureHeight_ = captureHeight;

  /**
   * Promise for ongoing capture operation.
   * @type {?Promise}
   * @private
   */
  this.capture_ = null;
};

/**
 * Initiates video/photo capture operation.
 * @return {?Promise} Promise for ongoing capture operation.
 */
cca.views.camera.Mode.prototype.startCapture = function() {
  if (!this.capture_) {
    this.capture_ = this.start_().finally(() => this.capture_ = null);
  }
  return this.capture_;
};

/**
 * Stops the ongoing capture operation.
 * @async
 * @return {Promise} Promise for ongoing capture operation.
 */
cca.views.camera.Mode.prototype.stopCapture = async function() {
  this.stop_();
  return await this.capture_;
};

/**
 * Initiates video/photo capturing operation under this mode.
 * @async
 * @protected
 */
cca.views.camera.Mode.prototype.start_ = async function() {};

/**
 * Stops the ongoing capture operation under this mode.
 * @protected
 */
cca.views.camera.Mode.prototype.stop_ = function() {};

/**
 * Video mode capture controller.
 * @param {MediaStream} stream
 * @param {function(?Blob, boolean, string): Promise} doSavePicture
 * @constructor
 */
cca.views.camera.Video = function(stream, doSavePicture) {
  cca.views.camera.Mode.call(this, stream, doSavePicture, -1, -1);

  /**
   * Promise for play start sound delay.
   * @type {?Promise}
   * @private
   */
  this.startSound_ = null;

  /**
   * MediaRecorder object to record motion pictures.
   * @type {MediaRecorder}
   * @private
   */
  this.mediaRecorder_ = null;

  /**
   * Record-time for the elapsed recording time.
   * @type {cca.views.camera.RecordTime}
   * @private
   */
  this.recordTime_ = new cca.views.camera.RecordTime();

  // End of properties, seal the object.
  Object.seal(this);
};

cca.views.camera.Video.prototype = {
  __proto__: cca.views.camera.Mode.prototype,
};

/**
 * @override
 */
cca.views.camera.Video.prototype.start_ = async function() {
  this.startSound_ = cca.sound.play('#sound-rec-start');
  try {
    await this.startSound_;
  } finally {
    this.startSound_ = null;
  }

  if (this.mediaRecorder_ == null) {
    try {
      if (!MediaRecorder.isTypeSupported(
              cca.views.camera.Video.VIDEO_MIMETYPE)) {
        throw new Error('The preferred mimeType is not supported.');
      }
      this.mediaRecorder_ = new MediaRecorder(
          this.stream_, {mimeType: cca.views.camera.Video.VIDEO_MIMETYPE});
    } catch (e) {
      cca.toast.show('error_msg_record_start_failed');
      throw e;
    }
  }

  let blob;
  // Take of recording will be ended by another shutter click.
  try {
    blob = await this.createVideoBlob_();
  } catch (e) {
    cca.toast.show('error_msg_empty_recording');
    throw e;
  }
  cca.sound.play('#sound-rec-end');

  await this.doSavePicture_(
      blob, true, (new cca.models.Filenamer()).newVideoName());
};

/**
 * @override
 */
cca.views.camera.Video.prototype.stop_ = function() {
  if (this.startSound_ && this.startSound_.cancel) {
    this.startSound_.cancel();
  }
  if (this.mediaRecorder_ && this.mediaRecorder_.state == 'recording') {
    this.mediaRecorder_.stop();
  }
};

/**
 * Video recording MIME type. Mkv with AVC1 is the only preferred
 * format.
 * @type {string}
 * @const
 */
cca.views.camera.Video.VIDEO_MIMETYPE = 'video/x-matroska;codecs=avc1';

/**
 * Starts a recording to create a blob of it after the recorder is stopped.
 * @return {!Promise<!Blob>} Promise for the result.
 * @private
 */
cca.views.camera.Video.prototype.createVideoBlob_ = function() {
  return new Promise((resolve, reject) => {
    var recordedChunks = [];
    var ondataavailable = (event) => {
      // TODO(yuli): Handle insufficient storage.
      if (event.data && event.data.size > 0) {
        recordedChunks.push(event.data);
      }
    };
    var onstop = (event) => {
      this.mediaRecorder_.removeEventListener('dataavailable', ondataavailable);
      this.mediaRecorder_.removeEventListener('stop', onstop);

      var recordedBlob = new Blob(
          recordedChunks, {type: cca.views.camera.Video.VIDEO_MIMETYPE});
      recordedBlob.mins = this.recordTime_.stop();
      recordedChunks = [];
      if (recordedBlob.size) {
        resolve(recordedBlob);
      } else {
        reject(new Error('Video blob error.'));
      }
    };
    this.mediaRecorder_.addEventListener('dataavailable', ondataavailable);
    this.mediaRecorder_.addEventListener('stop', onstop);

    // Start recording and update the UI for the ongoing recording.
    // TODO(yuli): Don't re-enable audio after crbug.com/878255 fixed in M73.
    var track = this.stream_.getAudioTracks()[0];
    var enableAudio = (enabled) => {
      if (track) {
        track.enabled = enabled;
      }
    };
    enableAudio(true);
    this.mediaRecorder_.start();
    enableAudio(cca.state.get('mic'));
    this.recordTime_.start();
  });
};

/**
 * Photo mode capture controller.
 * @param {MediaStream} stream
 * @param {function(?Blob, boolean, string): Promise} doSavePicture
 * @param {number} captureWidth
 * @param {number} captureHeight
 * @constructor
 */
cca.views.camera.Photo = function(
    stream, doSavePicture, captureWidth, captureHeight) {
  cca.views.camera.Mode.call(
      this, stream, doSavePicture, captureWidth, captureHeight);

  /**
   * ImageCapture object to capture still photos.
   * @type {?ImageCapture}
   * @private
   */
  this.imageCapture_ = null;
};

cca.views.camera.Photo.prototype = {
  __proto__: cca.views.camera.Mode.prototype,
};

/**
 * @override
 */
cca.views.camera.Photo.prototype.start_ = async function() {
  if (this.imageCapture_ == null) {
    try {
      this.imageCapture_ = new ImageCapture(this.stream_.getVideoTracks()[0]);
    } catch (e) {
      cca.toast.show('error_msg_take_photo_failed');
      throw e;
    }
  }

  let blob;
  try {
    blob = await this.createPhotoBlob_();
  } catch (e) {
    cca.toast.show('error_msg_take_photo_failed');
    throw e;
  }
  cca.sound.play('#sound-shutter');
  await this.doSavePicture_(
      blob, false, (new cca.models.Filenamer()).newImageName());
};

/**
 * Takes a photo to create a blob of it.
 * @async
 * @return {!Promise<!Blob>} Result blob.
 * @private
 */
cca.views.camera.Photo.prototype.createPhotoBlob_ = async function() {
  const photoSettings = {
    imageWidth: this.captureWidth_,
    imageHeight: this.captureHeight_,
  };
  return await this.imageCapture_.takePhoto(photoSettings);
};

/**
 * Square mode capture controller.
 * @param {MediaStream} stream
 * @param {function(?Blob, boolean, string): Promise} doSavePicture
 * @param {number} captureWidth
 * @param {number} captureHeight
 * @constructor
 */
cca.views.camera.Square = function(
    stream, doSavePicture, captureWidth, captureHeight) {
  cca.views.camera.Photo.call(
      this, stream, doSavePicture, captureWidth, captureHeight);

  /**
   * Picture saving callback from parent.
   * @type {function(?Blob, boolean, string): Promise}
   * @private
   */
  this.doAscentSave_ = this.doSavePicture_;

  // End of properties, seal the object.
  Object.seal(this);

  this.doSavePicture_ = async (blob, ...args) => {
    if (blob) {
      blob = await this.cropSquare(blob);
    }
    await this.doAscentSave_(blob, ...args);
  };
};

cca.views.camera.Square.prototype = {
  __proto__: cca.views.camera.Photo.prototype,
};

/**
 * Crops out maximum possible centered square from the image blob.
 * @param {Blob} blob
 * @return {Promise<Blob>} Promise with result cropped square image.
 */
cca.views.camera.Square.prototype.cropSquare = function(blob) {
  return new Promise((resolve, reject) => {
    var img = new Image();
    img.onload = () => {
      let side = Math.min(img.width, img.height);
      let canvas = document.createElement('canvas');
      canvas.width = side;
      canvas.height = side;
      let ctx = canvas.getContext('2d');
      ctx.drawImage(
          img, Math.floor((img.width - side) / 2),
          Math.floor((img.height - side) / 2), side, side, 0, 0, side, side);
      try {
        canvas.toBlob(resolve, 'image/jpeg');
      } catch (e) {
        reject(e);
      }
    };
    img.onerror = () => reject(new Error('Failed to load unprocessed image'));
    img.src = URL.createObjectURL(blob);
  });
};

/**
 * Portrait mode capture controller.
 * @param {MediaStream} stream
 * @param {function(?Blob, boolean): Promise} doSavePicture
 * @param {number} captureWidth
 * @param {number} captureHeight
 * @constructor
 */
cca.views.camera.Portrait = function(
    stream, doSavePicture, captureWidth, captureHeight) {
  cca.views.camera.Mode.call(
      this, stream, doSavePicture, captureWidth, captureHeight);

  /**
   * ImageCapture object to capture still photos.
   * @type {?cca.mojo.ImageCapture}
   * @private
   */
  this.crosImageCapture_ = null;

  // End of properties, seal the object.
  Object.seal(this);
};

cca.views.camera.Portrait.prototype = {
  __proto__: cca.views.camera.Mode.prototype,
};

/**
 * @override
 */
cca.views.camera.Portrait.prototype.start_ = async function() {
  if (this.crosImageCapture_ == null) {
    try {
      this.crosImageCapture_ =
          new cca.mojo.ImageCapture(this.stream_.getVideoTracks()[0]);
    } catch (e) {
      cca.toast.show('error_msg_take_photo_failed');
      throw e;
    }
  }
  const photoSettings = {
    imageWidth: this.captureWidth_,
    imageHeight: this.captureHeight_,
  };
  try {
    var [reference, portrait] = this.crosImageCapture_.takePhoto(
        photoSettings, [cros.mojom.Effect.PORTRAIT_MODE]);
  } catch (e) {
    cca.toast.show('error_msg_take_photo_failed');
    throw e;
  }
  let filenamer = new cca.models.Filenamer();
  let playSound = false;
  const [refSave, portraitSave] = [reference, portrait].map(async (p) => {
    const isPortrait = Object.is(p, portrait);
    try {
      var blob = await p;
    } catch (e) {
      cca.toast.show(
          isPortrait ? 'error_msg_take_portrait_photo_failed' :
                       'error_msg_take_photo_failed');
      throw e;
    }
    if (!playSound) {
      playSound = true;
      cca.sound.play('#sound-shutter');
    }
    await this.doSavePicture_(blob, true, filenamer.newBurstName(!isPortrait));
  });
  try {
    await portraitSave;
  } catch (e) {
    // Portrait image may failed due to absence of human faces.
    // TODO(inker): Log non-intended error.
  }
  await refSave;
};
