// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Parts Copyright 2005-2009, the Dojo Foundation. Used under the terms of the
// "New" BSD License:
//
//  http://download.dojotoolkit.org/release-1.3.2/dojo-release-1.3.2/dojo/LICENSE
//

/**
 * @fileoverview CFInstance.js provides a set of utilities for managing
 * ChromeFrame plugins, including creation, RPC services, and a singleton to
 * use for communicating from ChromeFrame hosted content to an external
 * CFInstance wrapper. CFInstance.js is stand-alone, designed to be served from
 * a CDN, and built to not create side-effects for other hosted content.
 * @author slightlyoff@google.com (Alex Russell)
 */

(function(scope) {
  // TODO:
  //   * figure out if there's any way to install w/o a browser restart, and if
  //     so, where and how
  //   * slim down Deferred and RPC scripts
  //   * determine what debugging APIs should be exposed and how they should be
  //     surfaced. What about content authoring in Chrome instances? Stubbing
  //     the other side of RPC's?

  // bail if we'd be over-writing an existing CFInstance object
  if (scope['CFInstance']) {
    return;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Utiliity and Cross-Browser Functions
  /////////////////////////////////////////////////////////////////////////////

  // a monotonically incrementing counter
  var _counter = 0;

  var undefStr = 'undefined';

  //
  // Browser detection: ua.isIE, ua.isSafari, ua.isOpera, etc.
  //

  /**
   * An object for User Agent detection
   * @type {!Object}
   * @protected
   */
  var ua = {};
  var n = navigator;
  var dua = String(n.userAgent);
  var dav = String(n.appVersion);
  var tv = parseFloat(dav);
  var duaParse = function(s){
    var c = 0;
    try {
      return parseFloat(
        dua.split(s)[1].replace(/\./g, function() {
          c++;
          return (c > 1) ? '' : '.';
        } )
      );
    } catch(e) {
      // squelch to intentionally return undefined
    }
  };
  /** @type {number} */
  ua.isOpera = dua.indexOf('Opera') >= 0 ? tv: undefined;
  /** @type {number} */
  ua.isWebKit = duaParse('WebKit/');
  /** @type {number} */
  ua.isChrome = duaParse('Chrome/');
  /** @type {number} */
  ua.isKhtml = dav.indexOf('KHTML') >= 0 ? tv : undefined;

  var index = Math.max(dav.indexOf('WebKit'), dav.indexOf('Safari'), 0);

  if (index && !ua.isChrome) {
    /** @type {number} */
    ua.isSafari = parseFloat(dav.split('Version/')[1]);
    if(!ua.isSafari || parseFloat(dav.substr(index + 7)) <= 419.3){
      ua.isSafari = 2;
    }
  }

  if (dua.indexOf('Gecko') >= 0 && !ua.isKhtml) {
    /** @type {number} */
    ua.isGecko = duaParse(' rv:');
  }

  if (ua.isGecko) {
    /** @type {number} */
    ua.isFF = parseFloat(dua.split('Firefox/')[1]) || undefined;
  }

  if (document.all && !ua.isOpera) {
    /** @type {number} */
    ua.isIE = parseFloat(dav.split('MSIE ')[1]) || undefined;
  }

 
  /**
   * Log out varargs to a browser-provided console object (if available). Else
   * a no-op.
   * @param {*} var_args Optional Things to log.
   * @protected
   **/
  var log = function() {
    if (window['console']) {
      try {
        if (ua.isSafari || ua.isChrome) {
          throw Error();
        }
        console.log.apply(console, arguments);
      } catch(e) { 
        try {
          console.log(toArray(arguments).join(' '));
        } catch(e2) {
          // squelch
        }
      }
    }
  };

  //
  // Language utility methods
  //
 
  /**
   * Determine if the passed item is a String
   * @param {*} item Item to test.
   * @protected
   **/
  var isString = function(item) {
    return typeof item == 'string';
  };

  /** 
   * Determine if the passed item is a Function object
   * @param {*} item Item to test.
   * @protected
   **/
  var isFunction = function(item) {
    return (
      item && (
        typeof item == 'function' || item instanceof Function
      )
    );
  };

  /** 
   * Determine if the passed item is an array.
   * @param {*} item Item to test.
   * @protected
   **/
  var isArray = function(item){
    return (
      item && (
        item instanceof Array || (
          typeof item == 'object' && 
          typeof item.length != undefStr
        )
      )
    );
  };

  /**
   * A toArray version which takes advantage of builtins
   * @param {*} obj The array-like object to convert to a real array.
   * @param {number} opt_offset An index to being copying from in the source.
   * @param {Array} opt_startWith An array to extend with elements of obj in
   *    lieu of creating a new array to return.
   * @private
   **/
  var _efficientToArray = function(obj, opt_offset, opt_startWith){
    return (opt_startWith || []).concat(
                              Array.prototype.slice.call(obj, opt_offset || 0 )
                             );
  };

  /**
   * A version of toArray that iterates in lieu of using array generics.
   * @param {*} obj The array-like object to convert to a real array.
   * @param {number} opt_offset An index to being copying from in the source.
   * @param {Array} opt_startWith An array to extend with elements of obj in
   * @private
   **/
  var _slowToArray = function(obj, opt_offset, opt_startWith){
    var arr = opt_startWith || []; 
    for(var x = opt_offset || 0; x < obj.length; x++){ 
      arr.push(obj[x]); 
    } 
    return arr;
  };

  /** 
   * Converts an array-like object (e.g., an "arguments" object) to a real
   * Array.
   * @param {*} obj The array-like object to convert to a real array.
   * @param {number} opt_offset An index to being copying from in the source.
   * @param {Array} opt_startWith An array to extend with elements of obj in
   * @protected
   */
  var toArray = ua.isIE ?  
    function(obj){
      return (
        obj.item ? _slowToArray : _efficientToArray
      ).apply(this, arguments);
    } :
    _efficientToArray;

  var _getParts = function(arr, obj, cb){
    return [ 
      isString(arr) ? arr.split('') : arr, 
      obj || window,
      isString(cb) ? new Function('item', 'index', 'array', cb) : cb
    ];
  };

  /** 
   * like JS1.6 Array.forEach()
   * @param {Array} arr the array to iterate
   * @param {function(Object, number, Array)} callback the method to invoke for
   *     each item in the array
   * @param {function?} thisObject Optional a scope to use with callback
   * @return {array} the original arr
   * @protected
   */
  var forEach = function(arr, callback, thisObject) {
    if(!arr || !arr.length){ 
      return arr;
    }
    var parts = _getParts(arr, thisObject, callback); 
    // parts has a structure of:
    //  [
    //    array,
    //    scope,
    //    function
    //  ]
    arr = parts[0];
    for (var i = 0, l = arr.length; i < l; ++i) { 
      parts[2].call( parts[1], arr[i], i, arr );
    }
    return arr;
  };

  /** 
   * returns a new function bound to scope with a variable number of positional
   * params pre-filled
   * @private
   */
  var _hitchArgs = function(scope, method /*,...*/) {
    var pre = toArray(arguments, 2);
    var named = isString(method);
    return function() {
      var args = toArray(arguments);
      var f = named ? (scope || window)[method] : method;
      return f && f.apply(scope || this, pre.concat(args));
    }
  };

  /** 
   * Like goog.bind(). Hitches the method (named or provided as a function
   * object) to scope, optionally partially applying positional arguments. 
   * @param {Object} scope the object to hitch the method to
   * @param {string|function} method the method to be bound
   * @return {function} The bound method
   * @protected
   */
  var hitch = function(scope, method){
    if (arguments.length > 2) {
      return _hitchArgs.apply(window, arguments);  // Function
    }

    if (!method) {
      method = scope;
      scope = null;
    }

    if (isString(method)) {
      scope = scope || window;
      if (!scope[method]) {
        throw(
          ['scope["', method, '"] is null (scope="', scope, '")'].join('')
        );
      }
      return function() {
        return scope[method].apply(scope, arguments || []);
      };
    }

    return !scope ?
        method :
        function() {
          return method.apply(scope, arguments || []);
        };
  };

  /** 
   * A version of addEventListener that works on IE too. *sigh*.
   * @param {!Object} obj The object to attach to
   * @param {!String} type Name of the event to attach to
   * @param {!Function} handler The function to connect
   * @protected
   */
  var listen = function(obj, type, handler) {
    if (obj['attachEvent']) {
      obj.attachEvent('on' + type, handler);
    } else {
      obj.addEventListener(type, handler, false);
    }
  };

  /** 
   * Adds "listen" and "_dispatch" methods to the passed object, taking
   * advantage of native event hanlding if it's available.
   * @param {Object} instance The object to install the event system on
   * @protected
   */
  var installEvtSys = function(instance) {
    var eventsMap = {};

    var isNative = (
      (typeof instance.addEventListener != undefStr) && 
      ((instance['tagName'] || '').toLowerCase()  != 'iframe')
    );

    instance.listen = function(type, func) {
      var t = eventsMap[type];
      if (!t) {
        t = eventsMap[type] = [];
        if (isNative) {
          listen(instance, type, hitch(instance, "_dispatch", type));
        }
      }
      t.push(func);
      return t;
    };

    instance._dispatch = function(type, evt) {
      var stopped = false;
      var stopper = function() {
        stopped = true;
      };

      forEach(eventsMap[type], function(f) {
        if (!stopped) {
          f(evt, stopper);
        }
      });
    };

    return instance;
  };

  /** 
   * Deserialize the passed JSON string
   * @param {!String} json A string to be deserialized
   * @return {Object}
   * @protected
   */
  var fromJson = window['JSON'] ? function(json) {
    return JSON.parse(json);
  } :
  function(json) {
    return eval('(' + (json || undefStr) + ')');
  };

  /** 
   * String escaping for use in JSON serialization
   * @param {string} str The string to escape
   * @return {string} 
   * @private
   */
  var _escapeString = function(str) {
    return ('"' + str.replace(/(["\\])/g, '\\$1') + '"').
             replace(/[\f]/g, '\\f').
             replace(/[\b]/g, '\\b').
             replace(/[\n]/g, '\\n').
             replace(/[\t]/g, '\\t').
             replace(/[\r]/g, '\\r').
             replace(/[\x0B]/g, '\\u000b'); // '\v' is not supported in JScript;
  };

  /** 
   * JSON serialization for arbitrary objects. Circular references or strong
   * typing information are not handled.
   * @param {Object} it Any valid JavaScript object or type
   * @return {string} the serialized representation of the passed object
   * @protected
   */
  var toJson = window['JSON'] ? function(it) {
    return JSON.stringify(it);
  } :
  function(it) {

    if (it === undefined) {
      return undefStr;
    }

    var objtype = typeof it;
    if (objtype == 'number' || objtype == 'boolean') {
      return it + '';
    }

    if (it === null) {
      return 'null';
    }

    if (isString(it)) { 
      return _escapeString(it); 
    }

    // recurse
    var recurse = arguments.callee;

    if(it.nodeType && it.cloneNode){ // isNode
      // we can't seriailize DOM nodes as regular objects because they have
      // cycles DOM nodes could be serialized with something like outerHTML,
      // but that can be provided by users in the form of .json or .__json__
      // function.
      throw new Error('Cannot serialize DOM nodes');
    }

    // array
    if (isArray(it)) {
      var res = [];
      forEach(it, function(obj) {
        var val = recurse(obj);
        if (typeof val != 'string') {
          val = undefStr;
        }
        res.push(val);
      });
      return '[' + res.join(',') + ']';
    }

    if (objtype == 'function') {
      return null;
    }

    // generic object code path
    var output = [];
    for (var key in it) {
      var keyStr, val;
      if (typeof key == 'number') {
        keyStr = '"' + key + '"';
      } else if(typeof key == 'string') {
        keyStr = _escapeString(key);
      } else {
        // skip non-string or number keys
        continue;
      }
      val = recurse(it[key]);
      if (typeof val != 'string') {
        // skip non-serializable values
        continue;
      }
      // TODO(slightlyoff): use += on Moz since it's faster there
      output.push(keyStr + ':' + val);
    }
    return '{' + output.join(',') + '}'; // String
  };

  // code to register with the earliest safe onload-style handler

  var _loadedListenerList = [];
  var _loadedFired = false;

  /** 
   * a default handler for document onload. When called (the first time),
   * iterates over the list of registered listeners, calling them in turn.
   * @private
   */
  var documentLoaded = function() {
    if (!_loadedFired) {
      _loadedFired = true;
      forEach(_loadedListenerList, 'item();');
    }
  };

  if (document.addEventListener) {
    // NOTE: 
    //    due to a threading issue in Firefox 2.0, we can't enable
    //    DOMContentLoaded on that platform. For more information, see:
    //    http://trac.dojotoolkit.org/ticket/1704
    if (ua.isWebKit > 525 || ua.isOpera || ua.isFF >= 3) {
      listen(document, 'DOMContentLoaded', documentLoaded);
    }
    //  mainly for Opera 8.5, won't be fired if DOMContentLoaded fired already.
    //  also used for FF < 3.0 due to nasty DOM race condition
    listen(window, 'load', documentLoaded);
  } else {
      // crazy hack for IE that relies on the "deferred" behavior of script
      // tags
      document.write(
        '<scr' + 'ipt defer src="//:" '
        + 'onreadystatechange="if(this.readyState==\'complete\')'
        +    '{ CFInstance._documentLoaded();}">'
        + '</scr' + 'ipt>'
      );
  }

  // TODO(slightlyoff): known KHTML init issues are ignored for now

  //
  // DOM utility methods
  //

  /** 
   * returns an item based on DOM ID. Optionally a doucment may be provided to
   * specify the scope to search in. If a node is passed, it's returned as-is.
   * @param {string|Node} id The ID of the node to be located or a node
   * @param {Node} doc Optional A document to search for id.
   * @return {Node} 
   * @protected
   */
  var byId = (ua.isIE || ua.isOpera) ?
    function(id, doc) {
      if (isString(id)) {
        doc = doc || document;
        var te = doc.getElementById(id);
        // attributes.id.value is better than just id in case the 
        // user has a name=id inside a form
        if (te && te.attributes.id.value == id) {
          return te;
        } else {
          var elements = doc.all[id];
          if (!elements || !elements.length) {
            return elements;
          }
          // if more than 1, choose first with the correct id
          var i=0;
          while (te = elements[i++]) {
            if (te.attributes.id.value == id) {
              return te;
            }
          }
        }
      } else {
        return id; // DomNode
      }
    } : 
    function(id, doc) {
      return isString(id) ? (doc || document).getElementById(id) : id;
    };


  /**
   * returns a unique DOM id which can be used to locate the node via byId().
   * If the node already has an ID, it's used. If not, one is generated. Like
   * IE's uniqueID property.
   * @param {Node} node The element to create or fetch a unique ID for
   * @return {String}
   * @protected
   */
  var getUid = function(node) {
    var u = 'cfUnique' + (_counter++);
    return (!node) ? u : ( node.id || node.uniqueID || (node.id = u) );
  };

  //
  // the Deferred class, borrowed from Twisted Python and Dojo
  //

  /** 
   * A class that models a single response (past or future) to a question.
   * Multiple callbacks and error handlers may be added. If the response was
   * added in the past, adding callbacks has the effect of calling them
   * immediately. In this way, Deferreds simplify thinking about synchronous
   * vs. asynchronous programming in languages which don't have continuations
   * or generators which might otherwise provide syntax for deferring
   * operations. 
   * @param {function} canceller Optional A function to be called when the
   *     Deferred is canceled.
   * @param {number} timeout Optional How long to wait (in ms) before errback
   *     is called with a timeout error. If no timeout is passed, the default
   *     is 1hr. Passing -1 will disable any timeout.
   * @constructor
   * @public
   */
  Deferred = function(/*Function?*/ canceller, timeout){
    //  example:
    //    var deferred = new Deferred();
    //    setTimeout(function(){ deferred.callback({success: true}); }, 1000);
    //    return deferred;
    this.chain = [];
    this.id = _counter++;
    this.fired = -1;
    this.paused = 0;
    this.results = [ null, null ];
    this.canceller = canceller;
    // FIXME(slightlyoff): is it really smart to be creating this many timers?
    if (typeof timeout == 'number') {
      if (timeout <= 0) {
        timeout = 216000;  // give it an hour
      }
    }
    this._timer = setTimeout(
                    hitch(this, 'errback', new Error('timeout')), 
                    (timeout || 10000)
                  );
    this.silentlyCancelled = false;
  };

  /**
   * Cancels a Deferred that has not yet received a value, or is waiting on
   * another Deferred as its value. If a canceller is defined, the canceller
   * is called. If the canceller did not return an error, or there was no
   * canceller, then the errback chain is started.
   * @public
   */
  Deferred.prototype.cancel = function() {
    var err;
    if (this.fired == -1) {
      if (this.canceller) {
        err = this.canceller(this);
      } else {
        this.silentlyCancelled = true;
      }
      if (this.fired == -1) {
        if ( !(err instanceof Error) ) {
          var res = err;
          var msg = 'Deferred Cancelled';
          if (err && err.toString) {
            msg += ': ' + err.toString();
          }
          err = new Error(msg);
          err.dType = 'cancel';
          err.cancelResult = res;
        }
        this.errback(err);
      }
    } else if (
      (this.fired == 0) &&
      (this.results[0] instanceof Deferred)
    ) {
      this.results[0].cancel();
    }
  };


  /**
   * internal function for providing a result. If res is an instance of Error,
   * we treat it like such and start the error chain.
   * @param {Object|Error} res the result
   * @private
   */
  Deferred.prototype._resback = function(res) {
    if (this._timer) {
      clearTimeout(this._timer);
    }
    this.fired = res instanceof Error ? 1 : 0;
    this.results[this.fired] = res;
    this._fire();
  };

  /**
   * determine if the deferred has already been resolved
   * @return {boolean}
   * @private
   */
  Deferred.prototype._check = function() {
    if (this.fired != -1) {
      if (!this.silentlyCancelled) {
        return 0;
      }
      this.silentlyCancelled = 0;
      return 1;
    }
    return 0;
  };

  /**
   * Begin the callback sequence with a non-error value.
   * @param {Object|Error} res the result
   * @public
   */
  Deferred.prototype.callback = function(res) {
    this._check();
    this._resback(res);
  };

  /**
   * Begin the callback sequence with an error result.
   * @param {Error|string} res the result. If not an Error, it's treated as the
   *     message for a new Error.
   * @public
   */
  Deferred.prototype.errback = function(res) {
    this._check();
    if ( !(res instanceof Error) ) {
      res = new Error(res);
    }
    this._resback(res);
  };

  /** 
   * Add a single function as the handler for both callback and errback,
   * allowing you to specify a scope (unlike addCallbacks).
   * @param {function|Object} cb A function. If cbfn is passed, the value of cb
   *     is treated as a scope
   * @param {function|string} cbfn Optional A function or name of a function in
   *     the scope cb.
   * @return {Deferred} this
   * @public
   */
  Deferred.prototype.addBoth = function(cb, cbfn) {
    var enclosed = hitch.apply(window, arguments);
    return this.addCallbacks(enclosed, enclosed);
  };

  /** 
   * Add a single callback to the end of the callback sequence. Add a function
   * as the handler for successful resolution of the Deferred. May be called
   * multiple times to register many handlers. Note that return values are
   * chained if provided, so it's best for callback handlers not to return
   * anything.
   * @param {function|Object} cb A function. If cbfn is passed, the value of cb
   *     is treated as a scope
   * @param {function|string} cbfn Optional A function or name of a function in
   *     the scope cb.
   * @return {Deferred} this
   * @public
   */
  Deferred.prototype.addCallback = function(cb, cbfn /*...*/) {
    return this.addCallbacks(hitch.apply(window, arguments));
  };


  /** 
   * Add a function as the handler for errors in the Deferred. May be called
   * multiple times to add multiple error handlers.
   * @param {function|Object} cb A function. If cbfn is passed, the value of cb
   *     is treated as a scope
   * @param {function|string} cbfn Optional A function or name of a function in
   *     the scope cb.
   * @return {Deferred} this
   * @public
   */
  Deferred.prototype.addErrback = function(cb, cbfn) {
    return this.addCallbacks(null, hitch.apply(window, arguments));
  };

  /** 
   * Add a functions as handlers for callback and errback in a single shot.
   * @param {function} callback A function
   * @param {function} errback A function 
   * @return {Deferred} this
   * @public
   */
  Deferred.prototype.addCallbacks = function(callback, errback) {
    this.chain.push([callback, errback]);
    if (this.fired >= 0) {
      this._fire();
    }
    return this;
  };

  /** 
   * when this Deferred is satisfied, pass it on to def, allowing it to run.
   * @param {Deferred} def A deferred to add to the end of this Deferred in a chain
   * @return {Deferred} this
   * @public
   */
  Deferred.prototype.chain = function(def) {
    this.addCallbacks(def.callback, def.errback);
    return this;
  };

  /** 
   * Used internally to exhaust the callback sequence when a result is
   * available.
   * @private
   */
  Deferred.prototype._fire = function() {
    var chain = this.chain;
    var fired = this.fired;
    var res = this.results[fired];
    var cb = null;
    while ((chain.length) && (!this.paused)) {
      var f = chain.shift()[fired];
      if (!f) {
        continue;
      }
      var func = hitch(this, function() {
        var ret = f(res);
        //If no response, then use previous response.
        if (typeof ret != undefStr) {
          res = ret;
        }
        fired = res instanceof Error ? 1 : 0;
        if (res instanceof Deferred) {
          cb = function(res) {
            this._resback(res);
            // inlined from _pause()
            this.paused--;
            if ( (this.paused == 0) && (this.fired >= 0)) {
              this._fire();
            }
          }
          // inlined from _unpause
          this.paused++;
        }
      });

      try {
        func.call(this);
      } catch(err) {
        fired = 1;
        res = err;
      }
    }

    this.fired = fired;
    this.results[fired] = res;
    if (cb && this.paused ) {
      // this is for "tail recursion" in case the dependent
      // deferred is already fired
      res.addBoth(cb);
    }
  };

  /////////////////////////////////////////////////////////////////////////////
  // Plugin Initialization Class and Helper Functions
  /////////////////////////////////////////////////////////////////////////////
  
  var returnFalse = function() {
    return false;
  };

  var cachedHasVideo;
  var cachedHasAudio;

  var contentTests = {
    canvas: function() {
      return !!(
        ua.isChrome || ua.isSafari >= 3 || ua.isFF >= 3 || ua.isOpera >= 9.2
      );
    },

    svg: function() {
      return !!(ua.isChrome || ua.isSafari || ua.isFF || ua.isOpera);
    },

    postMessage: function() {
      return (
        !!window['postMessage'] ||
        ua.isChrome ||
        ua.isIE >= 8 || 
        ua.isSafari >= 3 || 
        ua.isFF >= 3 || 
        ua.isOpera >= 9.2
      );
    },

    // the spec isn't settled and nothing currently supports it
    websocket: returnFalse,

    'css-anim': function() {
      // pretty much limited to WebKit's special transition and animation
      // properties. Need to figure out a better way to triangulate this as
      // FF3.x adds more of these properties in parallel.
      return ua.isWebKit > 500;
    },

    // "working" video/audio tag? 
    video: function() {
      if (typeof cachedHasVideo != undefStr) {
        return cachedHasVideo;
      }

      // We haven't figured it out yet, so probe the <video> tag and cache the
      // result.
      var video = document.createElement('video');
      return cachedHasVideo = (typeof video['play'] != undefStr);
    },

    audio: function() {
      if (typeof cachedHasAudio != undefStr) {
        return cachedHasAudio;
      }

      var audio = document.createElement('audio');
      return cachedHasAudio = (typeof audio['play'] != undefStr);
    },

    'video-theora': function() {
      return contentTests.video() && (ua.isChrome || ua.isFF > 3); 
    },

    'video-h264': function() {
      return contentTests.video() && (ua.isChrome || ua.isSafari >= 4); 
    },

    'audio-vorbis': function() {
      return contentTests.audio() && (ua.isChrome || ua.isFF > 3); 
    },

    'audio-mp3': function() {
      return contentTests.audio() && (ua.isChrome || ua.isSafari >= 4); 
    },

    // can we implement RPC over available primitives?
    rpc: function() {
      // on IE we need the src to be on the same domain or we need postMessage
      // to work. Since we can't count on the src being same-domain, we look
      // for things that have postMessage. We may re-visit this later and add
      // same-domain checking and cross-window-call-as-postMessage-replacement
      // code.

      // use "!!" to avoid null-is-an-object weirdness
      return !!window['postMessage'];
    },

    sql: function() {
      // HTML 5 databases
      return !!window['openDatabase'];
    },

    storage: function(){
      // DOM storage

      // IE8, Safari, etc. support "localStorage", FF supported "globalStorage"
      return !!window['globalStorage'] || !!window['localStorage'];
    }
  };

  // isIE, isFF, isWebKit, etc.
  forEach([
      'isOpera', 'isWebKit', 'isChrome', 'isKhtml', 'isSafari', 
      'isGecko', 'isFF', 'isIE'
    ], 
    function(name) {
      contentTests[name] = function() {
        return !!ua[name];
      };
    }
  );

  /** 
   * Checks the list of requirements to determine if the current host browser
   * meets them natively. Primarialy relies on the contentTests array.
   * @param {Array} reqs A list of tests, either names of test functions in
   *     contentTests or functions to execute.
   * @return {boolean} 
   * @private
   */
  var testRequirements = function(reqs) {
    // never use CF on Chrome or Safari
    if (ua.isChrome || ua.isSafari) {
      return true;
    }

    var allMatch = true;
    if (!reqs) {
      return false;
    }
    forEach(reqs, function(i) {
      var matches = false;
      if (isFunction(i)) {
        // support custom test functions
        matches = i();
      } else {
        // else it's a lookup by name
        matches = (!!contentTests[i] && contentTests[i]());
      }
      allMatch = allMatch && matches;
    });
    return allMatch;
  };

  var cachedAvailable;

  /** 
   * Checks to find out if ChromeFrame is available as a plugin
   * @return {Boolean} 
   * @private
   */
  var isCfAvailable = function() {
    if (typeof cachedAvailable != undefStr) {
      return cachedAvailable;
    }

    cachedAvailable = false;
    var p = n.plugins;
    if (typeof window['ActiveXObject'] != undefStr) {
      try {
        var i = new ActiveXObject('ChromeTab.ChromeFrame');
        if (i) {
          cachedAvailable = true;
        }
      } catch(e) {
        log('ChromeFrame not available, error:', e.message);
        // squelch
      }
    } else {
      for (var x = 0; x < p.length; x++) {
        if (p[x].name.indexOf('Google Chrome Frame') == 0) {
          cachedAvailable = true;
          break;
        }
      }
    }
    return cachedAvailable;
  };

  /** 
   * Creates a <param> element with the specified name and value. If a parent
   * is provided, the <param> element is appended to it.
   * @param {string} name The name of the param
   * @param {string} value The value
   * @param {Node} parent Optional parent element
   * @return {Boolean} 
   * @private
   */
  var param = function(name, value, parent) {
    var p = document.createElement('param');
    p.setAttribute('name', name);
    p.setAttribute('value', value);
    if (parent) {
      parent.appendChild(p);
    }
    return p;
  };

  /** @type {boolean} */
  var cfStyleTagInjected = false;

  /** 
   * Creates a style sheet in the document which provides default styling for
   * ChromeFrame instances. Successive calls should have no additive effect.
   * @private
   */
  var injectCFStyleTag = function() {
    if (cfStyleTagInjected) {
      // once and only once
      return;
    }
    try {
      var rule = ['.chromeFrameDefaultStyle {',
                    'width: 400px;',
                    'height: 300px;',
                    'padding: 0;',
                    'margin: 0;',
                  '}'].join('');
      var ss = document.createElement('style');
      ss.setAttribute('type', 'text/css');
      if (ss.styleSheet) {
        ss.styleSheet.cssText = rule;
      } else {
        ss.appendChild(document.createTextNode(rule));
      }
      var h = document.getElementsByTagName('head')[0];
      if (h.firstChild) {
        h.insertBefore(ss, h.firstChild);
      } else {
        h.appendChild(ss);
      }
      cfStyleTagInjected = true;
    } catch (e) {
      // squelch

      // FIXME(slightlyoff): log? retry?
    }
  };

  /** 
   * Plucks properties from the passed arguments and sets them on the passed
   * DOM node
   * @param {Node} node The node to set properties on
   * @param {Object} args A map of user-specified properties to set
   * @private
   */
  var setProperties = function(node, args) {
    injectCFStyleTag();

    var srcNode = byId(args['node']);

    node.id = args['id'] || (srcNode ? srcNode['id'] || getUid(srcNode) : '');

    // TODO(slightlyoff): Opera compat? need to test there
    var cssText = args['cssText'] || '';
    node.style.cssText = ' ' + cssText;

    var classText = args['className'] || '';
    node.className = 'chromeFrameDefaultStyle ' + classText;

    // default if the browser doesn't so we don't show sad-tab
    var src = args['src'] || 'about:blank';

    if (ua.isIE || ua.isOpera) {
      node.src = src;
    } else {
      // crazyness regarding when things are set in NPAPI
      node.setAttribute('src', src);
    }

    if (srcNode) {
      srcNode.parentNode.replaceChild(node, srcNode);
    }
  };

  /** 
   * Creates a plugin instance, taking named parameters from the passed args.
   * @param {Object} args A bag of configuration properties, including values
   *    like 'node', 'cssText', 'className', 'id', 'src', etc.
   * @return {Node} 
   * @private
   */
  var makeCFPlugin = function(args) {
    var el; // the element
    if (!ua.isIE) {
      el = document.createElement('object');
      el.setAttribute("type", "application/chromeframe");
    } else {
      var dummy = document.createElement('span');
      dummy.innerHTML = [
        '<object codeBase="//www.google.com"',
          "type='application/chromeframe'",
          'classid="CLSID:E0A900DF-9611-4446-86BD-4B1D47E7DB2A"></object>'
      ].join(' ');
      el = dummy.firstChild;
    }
    setProperties(el, args);
    return el;
  };

  /** 
   * Creates an iframe in lieu of a ChromeFrame plugin, taking named parameters
   * from the passed args.
   * @param {Object} args A bag of configuration properties, including values
   *    like 'node', 'cssText', 'className', 'id', 'src', etc.
   * @return {Node} 
   * @private
   */
  var makeCFIframe = function(args) {
    var el = document.createElement('iframe');
    setProperties(el, args);
    // FIXME(slightlyoff):
    //    This is where we'll need to slot in "don't fire load events for
    //    fallback URL" logic.
    listen(el, 'load', hitch(el, '_dispatch', 'load'));
    return el;
  };


  var msgPrefix = 'CFInstance.rpc:';

  /** 
   * A class that provides the ability for widget-mode hosted content to more
   * easily call hosting-page exposed APIs (and vice versa). It builds upon the
   * message-passing nature of ChromeFrame to route messages to the other
   * side's RPC host and coordinate events such as 'RPC readyness', buffering
   * calls until both sides indicate they are ready to participate.
   * @constructor
   * @public
   */
  var RPC = function(instance) {
    this.initDeferred = new Deferred();

    this.instance = instance;

    instance.listen('message', hitch(this, '_handleMessage'));

    this._localExposed = {};
    this._doWithAckCallbacks = {};

    this._open = false;
    this._msgBacklog = [];

    this._initialized = false;
    this._exposeMsgBacklog = [];

    this._exposed = false;
    this._callRemoteMsgBacklog = [];

    this._inFlight = {};

    var sendLoadMsg = hitch(this, function(evt) {
      this.doWithAck('load').addCallback(this, function() {
        this._open = true;
        this._postMessageBacklog();
      });
    });

    if (instance['tagName']) {
      instance.listen('load', sendLoadMsg);
    } else {
      sendLoadMsg();
    }
  };

  RPC.prototype._postMessageBacklog = function() {
    if (this._open) {
      // forEach(this._msgBacklog, this._postMessage, this);
      // this._msgBacklog = [];
      while (this._msgBacklog.length) {
        var msg = this._msgBacklog.shift();
        this._postMessage(msg);
      }
    }
  };

  RPC.prototype._postMessage = function(msg, force) {
    if (!force && !this._open) {
      this._msgBacklog.push(msg);
    } else {
      // FIXME(slightlyoff): need to check domains list here!
      this.instance.postMessage(msgPrefix + msg, '*');
    }
  };

  RPC.prototype._doWithAck = function(what) {
    this._postMessage('doWithAckCallback:' + what, what == 'load');
  };

  RPC.prototype.doWithAck = function(what) {
    var d = new Deferred();
    this._doWithAckCallbacks[what] = d;
    this._postMessage('doWithAck:' + what, what == 'load');
    return d;
  };

  RPC.prototype._handleMessage = function(evt, stopper) {
    var d = String(evt.data);

    if (d.indexOf(msgPrefix) != 0) {
      // not for us, allow the event dispatch to continue...
      return;
    }

    // ...else we're the end of the line for this event
    stopper();

    // see if we know what type of message it is
    d = d.substr(msgPrefix.length);

    var cIndex = d.indexOf(':');

    var type = d.substr(0, cIndex);

    if (type == 'doWithAck') {
      this._doWithAck(d.substr(cIndex + 1));
      return;
    }

    var msgBody = d.substr(cIndex + 1);

    if (type == 'doWithAckCallback') {
      this._doWithAckCallbacks[msgBody].callback(1);
      return;
    }

    if (type == 'init') {
      return;
    }

    // All the other stuff we can do uses a JSON payload.
    var obj = fromJson(msgBody);

    if (type == 'callRemote') {

      if (obj.method && obj.params && obj.id) {

        var ret = {
          success: 0,
          returnId: obj.id
        };

        try {
          // Undefined isn't valid JSON, so use null as default value.
          ret.value = this._localExposed[ obj.method ](evt, obj) || null;
          ret.success = 1;
        } catch(e) {
          ret.error = e.message;
        }

        this._postMessage('callReturn:' + toJson(ret));
      }
    }

    if (type == 'callReturn') {
      // see if we're waiting on an outstanding RPC call, which
      // would be identified by returnId.
      var rid = obj['returnId'];
      if (!rid) {
        // throw an error?
        return;
      }
      var callWrap = this._inFlight[rid];
      if (!callWrap) {
        return;
      }

      if (obj.success) {
        callWrap.d.callback(obj['value'] || 1);
      } else {
        callWrap.d.errback(new Error(obj['error'] || 'unspecified RPC error'));
      }
      delete this._inFlight[rid];
    }

  };
  
  /** 
   * Makes a method visible to be called 
   * @param {string} name The name to expose the method at. 
   * @param {Function|string} method The function (or name of the function) to
   *     expose. If a name is provided, it's looked up from the passed scope.
   *     If no scope is provided, the global scope is queried for a function
   *     with that name.
   * @param {Object} scope Optional A scope to bind the passed method to. If
   *     the method parameter is specified by a string, the method is both
   *     located on the passed scope and bound to it.
   * @param {Array} domains Optional A list of domains in
   *     'http://example.com:8080' format which may call the given method.
   *     Currently un-implemented.
   * @public
   */
  RPC.prototype.expose = function(name, method, scope, domains) {
    scope = scope || window;
    method = isString(method) ? scope[method] : method;

    // local call proxy that the other side will hit when calling our method
    this._localExposed[name] = function(evt, obj) {
      return method.apply(scope, obj.params);
    };

    if (!this._initialized) {
      this._exposeMsgBacklog.push(arguments);
      return;
    }

    var a = [name, method, scope, domains];
    this._sendExpose.apply(this, a);
  };

  RPC.prototype._sendExpose = function(name) {
    // now tell the other side that we're advertising this method
    this._postMessage('expose:' + toJson({ name: name }));
  };


  /** 
   * Calls a remote method asynchronously and returns a Deferred object
   * representing the response.
   * @param {string} method Name of the method to call. Should be the same name
   *     which the other side has expose()'d.
   * @param {Array} params Optional A list of arguments to pass to the called
   *     method.  All elements in the list must be cleanly serializable to
   *     JSON.
   * @param {CFInstance.Deferred} deferred Optional A Deferred to use for
   *     reporting the response of the call. If no deferred is passed, a new
   *     Deferred is created and returned.
   * @return {CFInstance.Deferred} 
   * @public
   */
  RPC.prototype.callRemote = function(method, params, timeout, deferred) {
    var d = deferred || new Deferred(null, timeout || -1);

    if (!this._exposed) {
      var args = toArray(arguments);
      args.length = 3;
      args.push(d);
      this._callRemoteMsgBacklog.push(args);
      return d;
    }


    if (!method) {
      d.errback('no method provided!');
      return d;
    }

    var id = msgPrefix + (_counter++);

    // JSON-ify the whole bundle
    var callWrapper = {
      method: String(method), 
      params: params || [],
      id: id
    };
    var callJson = toJson(callWrapper);
    callWrapper.d = d;
    this._inFlight[id] = callWrapper;
    this._postMessage('callRemote:' + callJson);
    return d;
  };


  /** 
   * Tells the other side of the connection that we're ready to start receiving
   * calls. Returns a Deferred that is called back when both sides have
   * initialized and any backlogged messages have been sent. RPC users should
   * generally work to make sure that they call expose() on all of the methods
   * they'd like to make available to the other side *before* calling init()
   * @return {CFInstance.Deferred} 
   * @public
   */
  RPC.prototype.init = function() {
    var d = this.initDeferred;
    this.doWithAck('init').addCallback(this, function() {
      // once the init floodgates are open, send our backlogs one at a time,
      // with a little lag in the middle to prevent ordering problems
    
      this._initialized = true;
      while (this._exposeMsgBacklog.length) {
        this.expose.apply(this, this._exposeMsgBacklog.shift());
      }

      setTimeout(hitch(this, function(){

        this._exposed = true;
        while (this._callRemoteMsgBacklog.length) {
          this.callRemote.apply(this, this._callRemoteMsgBacklog.shift());
        }

        d.callback(1);

      }), 30);
      
    });
    return d;
  };

  // CFInstance design notes:
  //  
  //    The CFInstance constructor is only ever used in host environments. In
  //    content pages (things hosted by a ChromeFrame instance), CFInstance
  //    acts as a singleton which provides services like RPC for communicating
  //    with it's mirror-image object in the hosting environment.  We want the
  //    same methods and properties to be available on *instances* of
  //    CFInstance objects in the host env as on the singleton in the hosted
  //    content, despite divergent implementation. 
  //
  //    Further complicating things, CFInstance may specialize behavior
  //    internally based on whether or not it is communicationg with a fallback
  //    iframe or a 'real' ChromeFrame instance.

  var CFInstance; // forward declaration
  var h = window['externalHost'];
  var inIframe = (window.parent != window);

  if (inIframe) {
    h = window.parent;
  }

  var normalizeTarget = function(targetOrigin) {
    var l = window.location;
    if (!targetOrigin) {
      if (l.protocol != 'file:') {
        targetOrigin = l.protocol + '//' + l.host + "/";
      } else {
        // TODO(slightlyoff):
        //    is this secure enough? Is there another way to get messages
        //    flowing reliably across file-hosted documents?
        targetOrigin = '*';
      }
    }
    return targetOrigin;
  };

  var postMessageToDest = function(dest, msg, targetOrigin) {
    return dest.postMessage(msg, normalizeTarget(targetOrigin));
  };

  if (h) {
    //
    // We're loaded inside a ChromeFrame widget (or something that should look
    // like we were).
    //

    CFInstance = {};

    installEvtSys(CFInstance);

    // FIXME(slightlyoff):
    //    passing a target origin to externalHost's postMessage seems b0rked
    //    right now, so pass null instead. Will re-enable hitch()'d variant
    //    once that's fixed.

    // CFInstance.postMessage = hitch(null, postMessageToDest, h);

    CFInstance.postMessage = function(msg, targetOrigin) {
      return h.postMessage(msg, 
                           (inIframe ? normalizeTarget(targetOrigin) : null) );
    };

    // Attach to the externalHost's onmessage to proxy it in to CFInstance's
    // onmessage.
    var dispatchMsg = function(evt) {
      try {
        CFInstance._dispatch('message', evt);
      } catch(e) {
        log(e);
        // squelch
      }
    };
    if (inIframe) {
      listen(window, 'message', dispatchMsg);
    } else {
      h.onmessage = dispatchMsg;
    }

    CFInstance.rpc = new RPC(CFInstance);

    _loadedListenerList.push(function(evt) {
      CFInstance._dispatch('load', evt);
    });

  } else {
    //
    // We're the host document.
    //

    var installProperties = function(instance, args) {
      var s = instance.supportedEvents = ['load', 'message'];
      instance._msgPrefix = 'CFMessage:';

      installEvtSys(instance);

      instance.log = log;

      // set up an RPC instance
      instance.rpc = new RPC(instance);

      forEach(s, function(evt) {
        var l = args['on' + evt];
        if (l) {
          instance.listen(evt, l);
        }
      });

      var contentWindow = instance.contentWindow;

      // if it doesn't have a postMessage, route to/from the iframe's built-in
      if (typeof instance['postMessage'] == undefStr && !!contentWindow) {

        instance.postMessage = hitch(null, postMessageToDest, contentWindow);

        listen(window, 'message', function(evt) {
          if (evt.source == contentWindow) {
            instance._dispatch('message', evt);
          }
        });
      }

      return instance;
    };

    /** 
     * A class whose instances correspond to ChromeFrame instances. Passing an
     * arguments object to CFInstance helps parameterize the instance. 
     * @constructor
     * @public
     */
    CFInstance = function(args) {
      args = args || {};
      var instance;
      var success = false;

      // If we've been passed a CFInstance object as our source node, just
      // re-use it.
      if (args['node']) {
        var n = byId(args['node']);
        // Look for CF-specific properties.
        if (n && n.tagName == 'OBJECT' && n.success && n.rpc) {
          // Navigate, set styles, etc.
          setProperties(n, args);
          return n;
        }
      }

      var force = !!args['forcePlugin'];

      if (!force && testRequirements(args['requirements'])) {
        instance = makeCFIframe(args);
        success = true;
      } else if (isCfAvailable()) {
        instance = makeCFPlugin(args);
        success = true;
      } else {
        // else create an iframe but load the failure content and
        // not the 'normal' content

        // grab the fallback URL, and if none, use the 'click here
        // to install ChromeFrame' URL. Note that we only support
        // polling for install success if we're using the default
        // URL

        var fallback = '//www.google.com/chromeframe';

        args.src = args['fallback'] || fallback;
        instance = makeCFIframe(args);

        if (args.src == fallback) {
          // begin polling for install success.

          // TODO(slightlyoff): need to prevent firing of onload hooks!
          // TODO(slightlyoff): implement polling
          // TODO(slightlyoff): replacement callback?
          // TODO(slightlyoff): add flag to disable this behavior
        }
      }
      instance.success = success;

      installProperties(instance, args);

      return instance;
    };

    // compatibility shims for development-time. These mirror the methods that
    // are created on the CFInstance singleton if we detect that we're running
    // inside of CF.
    if (!CFInstance['postMessage']) {
      CFInstance.postMessage = function() {
        var args = toArray(arguments);
        args.unshift('CFInstance.postMessage:');
        log.apply(null, args);
      };
      CFInstance.listen = function() {
        // this space intentionally left blank
      };
    }
  }

  // expose some properties
  CFInstance.ua = ua;
  CFInstance._documentLoaded = documentLoaded;
  CFInstance.contentTests = contentTests;
  CFInstance.isAvailable = function(requirements) {
    var hasCf = isCfAvailable();
    return requirements ? (hasCf || testRequirements(requirements)) : hasCf;

  };
  CFInstance.Deferred = Deferred;
  CFInstance.toJson = toJson;
  CFInstance.fromJson = fromJson;
  CFInstance.log = log;

  // expose CFInstance to the external scope. We've already checked to make
  // sure we're not going to blow existing objects away.
  scope.CFInstance = CFInstance;

})( this['ChromeFrameScope'] || this );

// vim: shiftwidth=2:et:ai:tabstop=2
