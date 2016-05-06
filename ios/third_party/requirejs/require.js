/** vim: et:ts=4:sw=4:sts=4
 * @license RequireJS 2.2.0 Copyright jQuery Foundation and other contributors.
 * Released under MIT license, http://github.com/requirejs/requirejs/LICENSE
 */
//Not using strict: uneven strict support in browsers, #392, and causes
//problems with requirejs.exec()/transpiler plugins that may not be strict.
/*jslint regexp: true, nomen: true, sloppy: true */
/*global window, navigator, document, importScripts, setTimeout, opera */

goog.provide('__crWeb.webUIRequire');

goog.require('__crWeb.webUIModuleLoadNotifier');


__crWeb.webUIModuleLoadNotifier = new WebUIModuleLoadNotifier();


var requirejs, define;
(function() {
  var req, s,
      commentRegExp = /(\/\*([\s\S]*?)\*\/|([^:]|^)\/\/(.*)$)/mg,
      cjsRequireRegExp = /[^.]\s*require\s*\(\s*["']([^'"\s]+)["']\s*\)/g,
      op = Object.prototype,
      ostring = op.toString,
      hasOwn = op.hasOwnProperty,
      defContextName = '_',
      contexts = {},
      globalDefQueue = [];

  // Could match something like ')//comment', do not lose the prefix to
  // comment.
  function commentReplace(match, multi, multiText, singlePrefix) {
    return singlePrefix || '';
  }

  function isFunction(it) {
    return ostring.call(it) === '[object Function]';
  }

  function isArray(it) {
    return ostring.call(it) === '[object Array]';
  }

  /**
   * Helper function for iterating over an array. If the func returns
   * a true value, it will break out of the loop.
   */
  function each(ary, func) {
    if (ary) {
      var i;
      for (i = 0; i < ary.length; i += 1) {
        if (ary[i] && func(ary[i], i, ary)) {
          break;
        }
      }
    }
  }

  function hasProp(obj, prop) {
    return hasOwn.call(obj, prop);
  }

  function getOwn(obj, prop) {
    return hasProp(obj, prop) && obj[prop];
  }

  /**
   * Cycles over properties in an object and calls a function for each
   * property value. If the function returns a truthy value, then the
   * iteration is stopped.
   */
  function eachProp(obj, func) {
    var prop;
    for (prop in obj) {
      if (hasProp(obj, prop)) {
        if (func(obj[prop], prop)) {
          break;
        }
      }
    }
  }

  // Similar to Function.prototype.bind, but the 'this' object is specified
  // first, since it is easier to read/figure out what 'this' will be.
  function bind(obj, fn) {
    return function() {
      return fn.apply(obj, arguments);
    };
  }

  function defaultOnError(err) {
    throw err;
  }

  /**
   * Constructs an error with a pointer to an URL with more information.
   * @param {String} id the error ID that maps to an ID on a web page.
   * @param {String} message human readable error.
   * @param {Error} [err] the original error, if there is one.
   *
   * @returns {Error}
   */
  function makeError(id, msg, err, requireModules) {
    var e = new Error(msg + '\nhttp://requirejs.org/docs/errors.html#' + id);
    e.requireType = id;
    e.requireModules = requireModules;
    if (err) {
      e.originalError = err;
    }
    return e;
  }

  function newContext(contextName) {
    var inCheckLoaded, Module, context, handlers,
        checkLoadedTimeoutId,
        config = {
          // Defaults. Do not set a default for map
          // config to speed up normalize(), which
          // will run faster if there is no default.
          waitSeconds: 7,
          baseUrl: './'
        },
        registry = {},
        // registry of just enabled modules, to speed
        // cycle breaking code when lots of modules
        // are registered, but not activated.
        enabledRegistry = {},
        defQueue = [],
        defined = {},
        urlFetched = {},
        requireCounter = 1;

    /**
     * Creates a module mapping that includes, module name, and path.
     *
     * @param {String} name the module name
     * @param {String} [parentModuleMap] parent module map
     * for the module name, used to resolve relative names.
     * This is true if this call is done for a define() module ID.
     * @param {Boolean} applyMap: apply the map config to the ID.
     * Should only be true if this map is for a dependency.
     *
     * @returns {Object}
     */
    function makeModuleMap(name, parentModuleMap, applyMap) {
      var url,
          parentName = parentModuleMap ? parentModuleMap.name : null,
          isDefine = true;

      // If no name, then it means it is a require call, generate an
      // internal name.
      if (!name) {
        isDefine = false;
        name = '_@r' + (requireCounter += 1);
      }

      // Account for relative paths if there is a base name.
      url = context.nameToUrl(name);

      return {
        name: name,
        parentMap: parentModuleMap,
        url: url,
        isDefine: isDefine,
        id: name
      };
    }

    function getModule(depMap) {
      var id = depMap.id,
          mod = getOwn(registry, id);

      if (!mod) {
        mod = registry[id] = new context.Module(depMap);
      }

      return mod;
    }

    function on(depMap, name, fn) {
      var id = depMap.id,
          mod = getOwn(registry, id);

      if (hasProp(defined, id) && (!mod || mod.defineEmitComplete)) {
        if (name === 'defined') {
          fn(defined[id]);
        }
      } else {
        mod = getModule(depMap);
        if (mod.error && name === 'error') {
          fn(mod.error);
        } else {
          mod.on(name, fn);
        }
      }
    }

    function onError(err, errback) {
      var ids = err.requireModules,
          notified = false;

      if (errback) {
        errback(err);
      } else {
        each(ids, function(id) {
          var mod = getOwn(registry, id);
          if (mod) {
            // Set error on module, so it skips timeout checks.
            mod.error = err;
            if (mod.events.error) {
              notified = true;
              mod.emit('error', err);
            }
          }
        });

        if (!notified) {
          req.onError(err);
        }
      }
    }

    /**
     * Internal method to transfer globalQueue items to this context's
     * defQueue.
     */
    function takeGlobalQueue() {
      // Push all the globalDefQueue items into the context's defQueue
      if (globalDefQueue.length) {
        each(globalDefQueue, function(queueItem) {
          var id = queueItem[0];
          if (typeof id === 'string') {
            context.defQueueMap[id] = true;
          }
          defQueue.push(queueItem);
        });
        globalDefQueue = [];
      }
    }

    handlers = {
      'require': function(mod) {
        if (mod.require) {
          return mod.require;
        } else {
          return (mod.require = context.makeRequire());
        }
      },
      'exports': function(mod) {
        mod.usingExports = true;
        if (mod.map.isDefine) {
          if (mod.exports) {
            return (defined[mod.map.id] = mod.exports);
          } else {
            return (mod.exports = defined[mod.map.id] = {});
          }
        }
      },
      'module': function(mod) {
        if (mod.module) {
          return mod.module;
        } else {
          return (mod.module = {
            id: mod.map.id,
            uri: mod.map.url,
            config: function() {
              return getOwn(config.config, mod.map.id) || {};
            },
            exports: mod.exports || (mod.exports = {})
          });
        }
      }
    };

    function cleanRegistry(id) {
      // Clean up machinery used for waiting modules.
      delete registry[id];
      delete enabledRegistry[id];
    }

    function breakCycle(mod, traced, processed) {
      var id = mod.map.id;

      if (mod.error) {
        mod.emit('error', mod.error);
      } else {
        traced[id] = true;
        each(mod.depMaps, function(depMap, i) {
          var depId = depMap.id,
              dep = getOwn(registry, depId);

          // Only force things that have not completed
          // being defined, so still in the registry,
          // and only if it has not been matched up
          // in the module already.
          if (dep && !mod.depMatched[i] && !processed[depId]) {
            if (getOwn(traced, depId)) {
              mod.defineDep(i, defined[depId]);
              mod.check(); //pass false?
            } else {
              breakCycle(dep, traced, processed);
            }
          }
        });
        processed[id] = true;
      }
    }

    function checkLoaded() {
      var err,
          waitInterval = config.waitSeconds * 1000,
          // It is possible to disable the wait interval by using
          // waitSeconds of 0.
          expired = waitInterval &&
                    (context.startTime + waitInterval) < new Date().getTime(),
          noLoads = [],
          reqCalls = [],
          stillLoading = false,
          needCycleCheck = true;

      // Do not bother if this call was a result of a cycle break.
      if (inCheckLoaded) {
        return;
      }

      inCheckLoaded = true;

      // Figure out the state of all the modules.
      eachProp(enabledRegistry, function(mod) {
        var map = mod.map,
            modId = map.id;

        // Skip things that are not enabled or in error state.
        if (!mod.enabled) {
          return;
        }

        if (!map.isDefine) {
          reqCalls.push(mod);
        }

        if (!mod.error) {
          // If the module should be executed, and it has not
          // been inited and time is up, remember it.
          if (!mod.inited && expired) {
            noLoads.push(modId);
          } else if (!mod.inited && mod.fetched && map.isDefine) {
            stillLoading = true;
            return (needCycleCheck = false);
          }
        }
      });

      if (expired && noLoads.length) {
        // If wait time expired, throw error of unloaded modules.
        err = makeError('timeout',
                        'Load timeout for modules: ' + noLoads,
                        null,
                        noLoads);
        err.contextName = context.contextName;
        return onError(err);
      }

      // Not expired, check for a cycle.
      if (needCycleCheck) {
        each(reqCalls, function(mod) {
          breakCycle(mod, {}, {});
        });
      }

      // If still waiting on loads, and the waiting load is something
      // other than a plugin resource, or there are still outstanding
      // scripts, then just try back later.
      if (!expired && stillLoading) {
        // Something is still waiting to load. Wait for it, but only
        // if a timeout is not already in effect.
        if (!checkLoadedTimeoutId) {
          checkLoadedTimeoutId = setTimeout(function() {
            checkLoadedTimeoutId = 0;
            checkLoaded();
          }, 50);
        }
      }

      inCheckLoaded = false;
    }

    Module = function(map) {
      this.events = {};
      this.map = map;
      this.depExports = [];
      this.depMaps = [];
      this.depMatched = [];
      this.depCount = 0;

      /* this.exports this.factory
         this.depMaps = [],
         this.enabled, this.fetched
      */
    };

    Module.prototype = {
      init: function(depMaps, factory, errback, options) {
        options = options || {};

        // Do not do more inits if already done. Can happen if there
        // are multiple define calls for the same module. That is not
        // a normal, common case, but it is also not unexpected.
        if (this.inited) {
          return;
        }

        this.factory = factory;

        if (errback) {
          // Register for errors on this module.
          this.on('error', errback);
        } else if (this.events.error) {
          // If no errback already, but there are error listeners
          // on this module, set up an errback to pass to the deps.
          errback = bind(this, function(err) {
            this.emit('error', err);
          });
        }

        // Do a copy of the dependency array, so that
        // source inputs are not modified.
        this.depMaps = depMaps && depMaps.slice(0);

        this.errback = errback;

        // Indicate this module has be initialized
        this.inited = true;

        this.ignore = options.ignore;

        // Could have option to init this module in enabled mode,
        // or could have been previously marked as enabled. However,
        // the dependencies are not known until init is called. So
        // if enabled previously, now trigger dependencies as enabled.
        if (options.enabled || this.enabled) {
          // Enable this module and dependencies.
          // Will call this.check()
          this.enable();
        } else {
          this.check();
        }
      },

      defineDep: function(i, depExports) {
        // Because of cycles, defined callback for a given
        // export can be called more than once.
        if (!this.depMatched[i]) {
          this.depMatched[i] = true;
          this.depCount -= 1;
          this.depExports[i] = depExports;
        }
      },

      fetch: function() {
        if (this.fetched) {
          return;
        }
        this.fetched = true;

        context.startTime = (new Date()).getTime();

        return this.load();
      },

      load: function() {
        var url = this.map.url;

        // Regular dependency.
        if (!urlFetched[url]) {
          urlFetched[url] = true;
          context.load(this.map.id, url);
        }
      },

      /**
       * Checks if the module is ready to define itself, and if so,
       * define it.
       */
      check: function() {
        if (!this.enabled || this.enabling) {
          return;
        }

        var err, cjsModule,
            id = this.map.id,
            depExports = this.depExports,
            exports = this.exports,
            factory = this.factory;

        if (!this.inited) {
          // Only fetch if not already in the defQueue and not 'main' module.
          if (!hasProp(context.defQueueMap, id) && this.map.name != 'main') {
            this.fetch();
          }
        } else if (this.error) {
          this.emit('error', this.error);
        } else if (!this.defining) {
          // The factory could trigger another require call
          // that would result in checking this module to
          // define itself again. If already in the process
          // of doing that, skip this work.
          this.defining = true;

          if (this.depCount < 1 && !this.defined) {
            if (isFunction(factory)) {
              // If there is an error listener, favor passing
              // to that instead of throwing an error. However,
              // only do it for define()'d  modules. require
              // errbacks should not be called for failures in
              // their callbacks (#699). However if a global
              // onError is set, use that.
              if ((this.events.error && this.map.isDefine) ||
                  req.onError !== defaultOnError) {
                try {
                  exports = context.execCb(id, factory, depExports, exports);
                } catch (e) {
                  err = e;
                }
              } else {
                exports = context.execCb(id, factory, depExports, exports);
              }

              // Favor return value over exports. If node/cjs in
              // play, then will not have a return value anyway.
              // Favor module.exports assignment over exports
              // object.
              if (this.map.isDefine && exports === undefined) {
                cjsModule = this.module;
                if (cjsModule) {
                  exports = cjsModule.exports;
                } else if (this.usingExports) {
                  // exports already set the defined value.
                  exports = this.exports;
                }
              }

              if (err) {
                err.requireMap = this.map;
                err.requireModules = this.map.isDefine ? [this.map.id] : null;
                err.requireType = this.map.isDefine ? 'define' : 'require';
                return onError((this.error = err));
              }
            } else {
              // Just a literal value
              exports = factory;
            }

            this.exports = exports;

            if (this.map.isDefine && !this.ignore) {
              defined[id] = exports;

              if (req.onResourceLoad) {
                var resLoadMaps = [];
                each(this.depMaps, function(depMap) {
                  resLoadMaps.push(depMap.normalizedMap || depMap);
                });
                req.onResourceLoad(context, this.map, resLoadMaps);
              }
            }

            // Clean up
            cleanRegistry(id);

            this.defined = true;
          }

          // Finished the define stage. Allow calling check again
          // to allow define notifications below in the case of a
          // cycle.
          this.defining = false;

          if (this.defined && !this.defineEmitted) {
            this.defineEmitted = true;
            this.emit('defined', this.exports);
            this.defineEmitComplete = true;
          }
        }
      },

      enable: function() {
        enabledRegistry[this.map.id] = this;
        this.enabled = true;

        // Set flag mentioning that the module is enabling,
        // so that immediate calls to the defined callbacks
        // for dependencies do not trigger inadvertent load
        // with the depCount still being zero.
        this.enabling = true;

        // Enable each dependency
        each(this.depMaps, bind(this, function(depMap, i) {
          var id, mod, handler;

          if (typeof depMap === 'string') {
            // Dependency needs to be converted to a depMap
            // and wired up to this module.
            depMap = makeModuleMap(
                depMap,
                (this.map.isDefine ? this.map : this.map.parentMap),
                true);
            this.depMaps[i] = depMap;

            this.depCount += 1;

            on(depMap, 'defined', bind(this, function(depExports) {
              if (this.undefed) {
                return;
              }
              this.defineDep(i, depExports);
              this.check();
            }));

            if (this.errback) {
              on(depMap, 'error', bind(this, this.errback));
            } else if (this.events.error) {
              // No direct errback on this module, but something
              // else is listening for errors, so be sure to
              // propagate the error correctly.
              on(depMap, 'error', bind(this, function(err) {
                this.emit('error', err);
              }));
            }
          }

          id = depMap.id;
          mod = registry[id];

          // Skip special modules like 'require', 'exports', 'module'
          // Also, don't call enable if it is already enabled,
          // important in circular dependency cases.
          if (!hasProp(handlers, id) && mod && !mod.enabled) {
            context.enable(depMap, this);
          }
        }));

        this.enabling = false;

        this.check();
      },

      on: function(name, cb) {
        var cbs = this.events[name];
        if (!cbs) {
          cbs = this.events[name] = [];
        }
        cbs.push(cb);
      },

      emit: function(name, evt) {
        each(this.events[name], function(cb) {
          cb(evt);
        });
        if (name === 'error') {
          // Now that the error handler was triggered, remove
          // the listeners, since this broken Module instance
          // can stay around for a while in the registry.
          delete this.events[name];
        }
      }
    };

    function callGetModule(args) {
      // Skip modules already defined.
      if (!hasProp(defined, args[0])) {
        getModule(makeModuleMap(args[0], null)).init(args[1], args[2]);
      }
    }

    function intakeDefines() {
      var args;

      // Any defined modules in the global queue, intake them now.
      takeGlobalQueue();

      // Make sure any remaining defQueue items get properly processed.
      while (defQueue.length) {
        args = defQueue.shift();
        if (args[0] === null) {
          return onError(makeError('mismatch',
                                   'Mismatched anonymous define() module: ' +
                                   args[args.length - 1]));
        } else {
          // args are id, deps, factory. Should be normalized by the
          // define() function.
          callGetModule(args);
        }
      }
      context.defQueueMap = {};
    }

    context = {
      config: config,
      contextName: contextName,
      registry: registry,
      defined: defined,
      urlFetched: urlFetched,
      defQueue: defQueue,
      defQueueMap: {},
      Module: Module,
      makeModuleMap: makeModuleMap,
      nextTick: req.nextTick,
      onError: onError,

      /**
       * Set a configuration for the context.
       */
      configure: function() {
        // If there are any "waiting to execute" modules in the registry,
        // update the maps for them, since their info, like URLs to load,
        // may have changed.
        eachProp(registry, function(mod, id) {
          // If module already has init called, since it is too
          // late to modify them.
          if (!mod.inited) {
            mod.map = makeModuleMap(id, null);
          }
        });
      },

      makeRequire: function() {
        function localRequire(deps, callback, errback) {
          var id, map, requireMod;

          if (typeof deps === 'string') {
            if (isFunction(callback)) {
              // Invalid call
              return onError(makeError('requireargs', 'Invalid require call'),
                             errback);
            }

            // Synchronous access to one module. If require.get is
            // available (as in the Node adapter), prefer that.
            if (req.get) {
              return req.get(context, deps, null, localRequire);
            }

            // Normalize module name, if it contains . or ..
            map = makeModuleMap(deps, null, true);
            id = map.id;

            if (!hasProp(defined, id)) {
              return onError(makeError(
                                 'notloaded', 'Module name "' + id +
                                 '" has not been loaded yet for context: ' +
                                 contextName + '. Use require([])'));
            }
            return defined[id];
          }

          // Grab defines waiting in the global queue.
          intakeDefines();

          // Mark all the dependencies as needing to be loaded.
          context.nextTick(function() {
            // Some defines could have been added since the
            // require call, collect them.
            intakeDefines();

            requireMod = getModule(makeModuleMap(null));

            requireMod.init(deps, callback, errback, {
              enabled: true
            });

            checkLoaded();
          });

          return localRequire;
        }

        return localRequire;
      },

      /**
       * Called to enable a module if it is still in the registry
       * awaiting enablement. A second arg, parent, the parent module,
       * is passed in for context, when this method is overridden by
       * the optimizer. Not shown here to keep code compact.
       */
      enable: function(depMap) {
        var mod = getOwn(registry, depMap.id);
        if (mod) {
          getModule(depMap).enable();
        }
      },

      /**
       * Internal method used by environment adapters to complete a load event.
       * A load event could be a script load or just a load pass from a
       * synchronous load call.
       * @param {String} moduleName the name of the module to potentially
       *     complete.
       */
      completeLoad: function(moduleName) {
        var found, args, mod;
        takeGlobalQueue();

        while (defQueue.length) {
          args = defQueue.shift();
          if (args[0] === null) {
            args[0] = moduleName;
            // If already found an anonymous module and bound it
            // to this name, then this is some other anon module
            // waiting for its completeLoad to fire.
            if (found) {
              break;
            }
            found = true;
          } else if (args[0] === moduleName) {
            // Found matching define call for this script!
            found = true;
          }

          callGetModule(args);
        }
        context.defQueueMap = {};

        // Do this after the cycle of callGetModule in case the result
        // of those calls/init calls changes the registry.
        mod = getOwn(registry, moduleName);

        if (!found && !hasProp(defined, moduleName) && mod && !mod.inited) {
          // A script that does not call define(), so just simulate
          // the call for it.
          callGetModule([moduleName, [], null]);
        }

        checkLoaded();
      },

      /**
       * Converts a module name to a file path. Supports cases where
       * moduleName may actually be just an URL.
       */
      nameToUrl: function(moduleName, ext) {
        var syms, i, url, parentPath, bundleId;

        // If a colon is in the URL, it indicates a protocol is used and it
        // is just an URL to a file, or if it starts with a slash, contains a
        // query arg (i.e. ?) or ends with .js, then assume the user meant to
        // use an url and not a module id. The slash is important for
        // protocol-less URLs as well as full paths.
        if (req.jsExtRegExp.test(moduleName)) {
            // Just a plain path, not module name lookup, so just return it.
            // Add extension if it is included. This is a bit wonky, only
            // non-.js things pass an extension, this method probably needs
            // to be reworked.
            url = moduleName;
        } else {
          syms = moduleName.split('/');
          // Join the path parts together, then figure out if baseUrl is needed.
          url = syms.join('/');
          url += (/^data\:|\?/.test(url) || '.js');
          url = (url.charAt(0) === '/' ||
                 url.match(/^[\w\+\.\-]+:/) ? '' : config.baseUrl) + url;
        }

        return url;
      },

      // Delegates to req.load. Broken out as a separate function to
      // allow overriding in the optimizer.
      load: function(id, url) {
        req.load(context, id, url);
      },

      /**
       * Executes a module callback function. Broken out as a separate function
       * solely to allow the build system to sequence the files in the built
       * layer in the right sequence.
       *
       * @private
       */
      execCb: function(name, callback, args, exports) {
        return callback.apply(exports, args);
      },

      /**
       * Direct callback for load error.
       */
      onScriptFailure: function(moduleName) {
        onError(makeError('scripterror', 'Script error for "' +
                          moduleName + '"'), null, [moduleName]);
      },
    };

    context.require = context.makeRequire();
    return context;
  }

  /**
   * Main entry point.
   *
   * If the only argument to require is a string, then the module that
   * is represented by that string is fetched for the appropriate context.
   *
   * If the first argument is an array, then it will be treated as an array
   * of dependency string names to fetch. An optional function callback can
   * be specified to execute when all of those dependencies are available.
   *
   * Make a local req variable to help Caja compliance (it assumes things
   * on a require that are not standardized), and to give a short
   * name for minification/local scope use.
   */
  req = requirejs = function(deps, callback, errback, optional) {

    // Find the right context, use default
    var context, config,
        contextName = defContextName;

    // Determine if have config object in the call.
    if (!isArray(deps) && typeof deps !== 'string') {
      // deps is a config object
      config = deps;
      if (isArray(callback)) {
        // Adjust args if there are dependencies
        deps = callback;
        callback = errback;
        errback = optional;
      } else {
        deps = [];
      }
    }

    context = getOwn(contexts, contextName);
    if (!context) {
      context = contexts[contextName] = req.s.newContext(contextName);
    }

    if (config) {
      context.configure(config);
    }

    return context.require(deps, callback, errback);
  };

  /**
   * Execute something after the current tick
   * of the event loop. Override for other envs
   * that have a better solution than setTimeout.
   * @param  {Function} fn function to execute later.
   */
  req.nextTick = typeof setTimeout !== 'undefined' ? function(fn) {
    setTimeout(fn, 4);
  } : function(fn) { fn(); };

  // Used to filter out dependencies that are already paths.
  req.jsExtRegExp = /^\/|:|\?|\.js$/;
  s = req.s = {
    contexts: contexts,
    newContext: newContext
  };

  // Create default context.
  req({});

  /**
   * Any errors that require explicitly generates will be passed to this
   * function. Intercept/override it if you want custom error handling.
   * @param {Error} err the error object.
   */
  req.onError = defaultOnError;

  /**
   * Does the request to load a module for the browser case.
   * Make this a separate function to allow other environments
   * to override it.
   *
   * @param {Object} context the require context to find state.
   * @param {String} moduleName the name of the module.
   * @param {Object} url the URL to the module.
   */
  req.load = function(context, moduleName, url) {
    var config = (context && context.config) || {};
    var loadId = __crWeb.webUIModuleLoadNotifier.addPendingContext(
        moduleName, context);
    chrome.send('webui.loadMojo', [moduleName, loadId]);
   };

  /**
   * The function that handles definitions of modules. Differs from
   * require() in that a string for the module should be the first argument,
   * and the function to execute after dependencies are loaded should
   * return a value to define the module corresponding to the first argument's
   * name.
   */
  define = function(name, deps, callback) {
    var node, context;

    // Allow for anonymous modules
    if (typeof name !== 'string') {
      // Adjust args appropriately
      callback = deps;
      deps = name;
      name = null;
    }

    // This module may not have dependencies
    if (!isArray(deps)) {
      callback = deps;
      deps = null;
    }

    // If no name, and callback is a function, then figure out if it a
    // CommonJS thing with dependencies.
    if (!deps && isFunction(callback)) {
      deps = [];
      // Remove comments from the callback string,
      // look for require calls, and pull them into the dependencies,
      // but only if there are function args.
      if (callback.length) {
        callback
            .toString()
            .replace(commentRegExp, commentReplace)
            .replace(cjsRequireRegExp, function(match, dep) {
                deps.push(dep);
            });

        // May be a CommonJS thing even without require calls, but still
        // could use exports, and module. Avoid doing exports and module
        // work though if it just needs require.
        // REQUIRES the function to expect the CommonJS variables in the
        // order listed below.
        deps = (callback.length === 1 ?
                    ['require'] :
                    ['require', 'exports', 'module']).concat(deps);
      }
    }

    // Always save off evaluating the def call until the script onload
    // handler. This allows multiple modules to be in a file without
    // prematurely tracing dependencies, and allows for anonymous module
    // support, where the module name is not known until the script onload
    // event occurs. If no context, use the global queue, and get it
    // processed in the onscript load callback.
    if (context) {
      context.defQueue.push([name, deps, callback]);
      context.defQueueMap[name] = true;
    } else {
      globalDefQueue.push([name, deps, callback]);
    }
  };

  req();
}(this));

document.addEventListener("DOMContentLoaded", function(event) {
  requirejs(['main'], function(main) {
    main();
  }, function(error) {
    throw error;
  });
});
