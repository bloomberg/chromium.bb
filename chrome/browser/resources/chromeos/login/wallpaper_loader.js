// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('login', function() {

  /**
   * Minimum wallpaper load delay in milliseconds.
   * @type {number}
   * @const
   */
  var WALLPAPER_LOAD_MIN_DELAY_MS = 100;

  /**
   * If last walpaper load time cannot be calculated, assume this value.
   * @type {number}
   * @const
   */
  var WALLPAPER_DEFAULT_LOAD_TIME_MS = 200;

  /**
   * Min and Max average wallpaper load time.
   * Delay to next wallpaper load is 2 * <average load time>.
   * @type {number}
   * @const
   */
  var WALLPAPER_MIN_LOAD_TIME_MS = 50;
  var WALLPAPER_MAX_LOAD_TIME_MS = 2000;

  /**
   * Number last wallpaper load times to remember.
   * @type {number}
   * @const
   */
  var WALLPAPER_LOAD_STATS_MAX_LENGTH = 4;


  /**
   * Creates a new pod row element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var WallpaperLoader = function() {
    this.wallpaperLoadInProgress_ = {
        name: '',
        date: undefined
    };
    this.loadStats_ = [];
  };

  WallpaperLoader.prototype = {
    // When moving through users quickly at login screen, set a timeout to
    // prevent loading intermediate wallpapers.
    loadWallpaperTimeout_: null,

    // When waiting for wallpaper load, remember load start time.
    // wallpaperLoadInProgress_: { name: '', date: undefined }
    wallpaperLoadInProgress_: undefined,

    // Wait a delay and then load this wallpaper. Value = username.
    wallpaperLoadPending_: undefined,

    // Wait untill this Date before loading next wallpaper.
    wallpaperLoadTryNextAfter_: undefined,

    // Username, owner of current wallpaper.
    currentWallpaper_: '',

    // Array of times (in milliseconds) of last wallpaper load attempts.
    // Length is limited by WALLPAPER_LOAD_STATS_MAX_LENGTH.
    loadStats_: undefined,

    /**
     * Stop load timer. Clear pending record.
     */
    reset: function() {
      delete this.wallpaperLoadPending_;
      if (this.loadWallpaperTimeout_ != null)
        window.clearTimeout(this.loadWallpaperTimeout_);
      this.loadWallpaperTimeout_ = null;
    },

    /**
     * Schedules wallpaper load.
     */
    scheduleLoad: function(email) {
      if (this.wallpaperLoadPending_ && this.wallpaperLoadPending_ == email)
        return;
      this.reset();
      if (this.wallpaperLoadInProgress_.name == email)
        return;
      if ((this.wallpaperLoadInProgress_.name == '') &&
          (this.currentWallpaper_ == email))
        return;

      this.wallpaperLoadPending_ = email;
      var now = new Date();
      var timeout = WALLPAPER_LOAD_MIN_DELAY_MS;
      if (this.wallpaperLoadTryNextAfter_)
        timeout = Math.max(timeout, this.wallpaperLoadTryNextAfter_ - now);

      this.loadWallpaperTimeout_ = window.setTimeout(
        this.loadWallpaper_.bind(this), timeout);
     },


    /**
     * Loads pending wallpaper, if any.
     * @private
     */
    loadWallpaper_: function() {
      this.loadWallpaperTimeout_ = null;
      if (!this.wallpaperLoadPending_)
        return;
      if (this.wallpaperLoadInProgress_.name != '')
        return;
      var email = this.wallpaperLoadPending_;
      delete this.wallpaperLoadPending_;
      if (email == this.currentWallpaper_)
        return;
      this.wallpaperLoadInProgress_.name = email;
      this.wallpaperLoadInProgress_.date = new Date();
      chrome.send('loadWallpaper', [email]);
    },

    /**
     * Calculates average wallpaper load time.
     */
    calcLoadStatsAvg: function() {
        return this.loadStats_.reduce(
            function(previousValue, currentValue) {
              return previousValue + currentValue;
            }) / this.loadStats_.length;
    },

    /**
     * Calculates average next wallpaper load delay time.
     */
    getWallpaperLoadTime_: function() {
      var avg = WALLPAPER_DEFAULT_LOAD_TIME_MS;

      if (this.loadStats_.length == 0)
        return avg;

      avg = this.calcLoadStatsAvg();
      if (avg < WALLPAPER_MIN_LOAD_TIME_MS)
        avg = WALLPAPER_MIN_LOAD_TIME_MS;

      if (avg > WALLPAPER_MAX_LOAD_TIME_MS)
        avg = WALLPAPER_MAX_LOAD_TIME_MS;

      return avg;
    },

    /**
     * Handles 'onWallpaperLoaded' event. Recalculates statistics and
     * [re]schedules next wallpaper load.
     */
    onWallpaperLoaded: function(email) {
      this.currentWallpaper_ = email;
      if (email != this.wallpaperLoadInProgress_.name)
          return;
      this.wallpaperLoadInProgress_.name = '';
      var started = this.wallpaperLoadInProgress_.date;
      var finished = new Date();
      var elapsed = started ? finished - started :
          WALLPAPER_DEFAULT_LOAD_TIME_MS;
      this.loadStats_.push(elapsed);
      if (this.loadStats_.length > WALLPAPER_LOAD_STATS_MAX_LENGTH)
        this.loadStats_.shift();

      this.wallpaperLoadTryNextAfter_ = new Date(Date.now() + 2 *
          this.getWallpaperLoadTime_());
      if (this.wallpaperLoadPending_) {
        var newWallpaperEmail = this.wallpaperLoadPending_;
        this.reset();
        this.scheduleLoad(newWallpaperEmail);
      }
    }
  };

  return {
    WallpaperLoader: WallpaperLoader
  };
});
