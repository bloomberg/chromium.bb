// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file is the controller for generating extension
 * doc pages.
 *
 * It expects to have available via XHR (relative path):
 *   1) API_TEMPLATE which is the main template for the api pages.
 *   2) A file located at SCHEMA which is shared with the extension system and
 *      defines the methods and events contained in one api.
 *   3) (Possibly) A static version of the current page url in /static/. I.e.
 *      if called as ../foo.html, it will look for ../static/foo.html.
 *
 * The "shell" page may have a renderering already contained within it so that
 * the docs can be indexed.
 *
 */

var API_TEMPLATE = "template/api_template.html";
var WEBKIT_PATH = "../../../../third_party/WebKit";
var SCHEMA = "../api/extension_api.json";
var DEVTOOLS_SCHEMA = WEBKIT_PATH +
  "/Source/WebCore/inspector/front-end/ExtensionAPISchema.json";
var USE_DEVTOOLS_SCHEMA =
  /\.webInspector[^/]*\.html/.test(location.pathname);
var API_MODULE_PREFIX = USE_DEVTOOLS_SCHEMA ? "" : "chrome.";
var SAMPLES = "samples.json";
var REQUEST_TIMEOUT = 2000;

function staticResource(name) { return "static/" + name + ".html"; }

// Base name of this page. (i.e. "tabs", "overview", etc...).
var pageBase;

// Data to feed as context into the template.
var pageData = {};

// The full extension api schema
var schema;

// List of Chrome extension samples.
var samples;

// Mappings of api calls to URLs
var apiMapping;

// The current module for this page (if this page is an api module);
var module;

// Mapping from typeId to module.
var typeModule = {};

// Auto-created page name as default
var pageName;

// If this page is an apiModule, the name of the api module
var apiModuleName;


// Visits each item in the list in-order. Stops when f returns any truthy
// value and returns that node.
Array.prototype.select = function(f) {
  for (var i = 0; i < this.length; i++) {
    if (f(this[i], i))
      return this[i];
  }
}

// Assigns all keys & values of |obj2| to |obj1|.
function extend(obj, obj2) {
  for (var k in obj2) {
    obj[k] = obj2[k];
  }
}

/*
 * Main entry point for composing the page. It will fetch it's template,
 * the extension api, and attempt to fetch the matching static content.
 * It will insert the static content, if any, prepare it's pageData then
 * render the template from |pageData|.
 */
function renderPage() {
  // The page name minus the ".html" extension.
  pageBase = document.location.href.match(/\/([^\/]*)\.html/)[1];
  if (!pageBase) {
    alert("Empty page name for: " + document.location.href);
    return;
  }

  pageName = pageBase.replace(/([A-Z])/g, " $1");
  pageName = pageName.substring(0, 1).toUpperCase() + pageName.substring(1);

  // Fetch the api template and insert into the <body>.
  fetchContent(API_TEMPLATE, function(templateContent) {
    document.getElementsByTagName("body")[0].innerHTML = templateContent;
    fetchStatic();
  }, function(error) {
    alert("Failed to load " + API_TEMPLATE + ". " + error);
  });
}

function fetchStatic() {
  // Fetch the static content and insert into the "static" <div>.
  fetchContent(staticResource(pageBase), function(overviewContent) {
    document.getElementById("static").innerHTML = overviewContent;
    fetchSchema();
  }, function(error) {
    // Not fatal. Some api pages may not have matching static content.
    fetchSchema();
  });
}

function fetchSchema() {
  // Now the page is composed with the authored content, we fetch the schema
  // and populate the templates.
  var is_experimental_index = /\/experimental\.html$/.test(location.pathname);

  var schemas_to_retrieve = [];
  if (!USE_DEVTOOLS_SCHEMA || is_experimental_index)
    schemas_to_retrieve.push(SCHEMA);
  if (USE_DEVTOOLS_SCHEMA || is_experimental_index)
    schemas_to_retrieve.push(DEVTOOLS_SCHEMA);

  var schemas_retrieved = 0;
  schema = [];

  function onSchemaContent(content) {
    schema = schema.concat(JSON.parse(content));
    if (++schemas_retrieved < schemas_to_retrieve.length)
      return;
    if (pageName.toLowerCase() == "samples") {
      fetchSamples();
    } else {
      renderTemplate();
    }
  }

  for (var i = 0; i < schemas_to_retrieve.length; ++i) {
    var schema_path = schemas_to_retrieve[i];
    fetchContent(schema_path, onSchemaContent, function(error) {
      alert("Failed to load " + schema_path);
    });
  }
}

function fetchSamples() {
  // If we're rendering the samples directory, fetch the samples manifest.
  fetchContent(SAMPLES, function(sampleManifest) {
    var data = JSON.parse(sampleManifest);
    samples = data.samples;
    apiMapping = data.api;
    renderTemplate();
  }, function(error) {
    renderTemplate();
  });
}

/**
 * Fetches |url| and returns it's text contents from the xhr.responseText in
 * onSuccess(content)
 */
function fetchContent(url, onSuccess, onError) {
  var localUrl = url;
  var xhr = new XMLHttpRequest();
  var abortTimerId = window.setTimeout(function() {
    xhr.abort();
    console.log("XHR Timed out");
  }, REQUEST_TIMEOUT);

  function handleError(error) {
    window.clearTimeout(abortTimerId);
    if (onError) {
      onError(error);
      // Some cases result in multiple error handings. Only fire the callback
      // once.
      onError = undefined;
    }
  }

  try {
    xhr.onreadystatechange = function(){
      if (xhr.readyState == 4) {
        if (xhr.status < 300 && xhr.responseText) {
          window.clearTimeout(abortTimerId);
          onSuccess(xhr.responseText);
        } else {
          handleError("Failure to fetch content");
        }
      }
    }

    xhr.onerror = handleError;

    xhr.open("GET", url, true);
    xhr.send(null);
  } catch(e) {
    console.log("ex: " + e);
    console.error("exception: " + e);
    handleError();
  }
}

function renderTemplate() {
  schema.forEach(function(mod) {
    if (mod.namespace == pageBase) {
      // Do not render page for modules which are marked as "nodoc": true.
      if (mod.nodoc) {
        return;
      }
      // This page is an api page. Setup types and apiDefinition.
      module = mod;
      apiModuleName = API_MODULE_PREFIX + module.namespace;
      pageData.apiDefinition = module;
    }

    if (mod.types) {
      mod.types.forEach(function(type) {
        typeModule[type.id] = mod;
      });
    }
  });

  /**
   * Special pages like the samples gallery may want to modify their template
   * data to include additional information.  This hook allows a page template
   * to specify code that runs in the context of the api_page_generator.js
   * file before the jstemplate is rendered.
   *
   * To specify such code, the page template should include a script block with
   * a type of "text/prerenderjs" containing the code to be executed.  Note that
   * linking to an external file is not supported - code must be accessible
   * via the script block's innerText property.
   *
   * Code that is run this way may modify the data sent to jstemplate by
   * modifying the window.pageData variable.  This code will also have access
   * to any methods declared in the api_page_generator.js file.  The code
   * does not need to return any specific value to function.
   *
   * Note that code specified in this manner will be removed before the
   * template is rendered, and will therefore not be exposed to the end user
   * in the final rendered template.
   */
  var preRender = document.querySelector('script[type="text/prerenderjs"]');
  if (preRender) {
    preRender.parentElement.removeChild(preRender);
    eval(preRender.innerText);
  }

  // Render to template
  var input = new JsEvalContext(pageData);
  var output = document.getElementsByTagName("body")[0];
  jstProcess(input, output);

  selectCurrentPageOnLeftNav();

  document.title = getPageTitle();
  // Show
  if (window.postRender)
    window.postRender();

  if (parent && parent.done)
    parent.done();
}

function removeJsTemplateAttributes(root) {
  var jsattributes = ["jscontent", "jsselect", "jsdisplay", "transclude",
                      "jsvalues", "jsvars", "jseval", "jsskip", "jstcache",
                      "jsinstance"];

  var nodes = root.getElementsByTagName("*");
  for (var i = 0; i < nodes.length; i++) {
    var n = nodes[i]
    jsattributes.forEach(function(attributeName) {
      n.removeAttribute(attributeName);
    });
  }
}

function serializePage() {
 removeJsTemplateAttributes(document);
 var s = new XMLSerializer();
 return s.serializeToString(document);
}

function evalXPathFromNode(expression, node) {
  var results = document.evaluate(expression, node, null,
      XPathResult.ORDERED_NODE_ITERATOR_TYPE, null);
  var retval = [];
  while(n = results.iterateNext()) {
    retval.push(n);
  }

  return retval;
}

function evalXPathFromId(expression, id) {
  return evalXPathFromNode(expression, document.getElementById(id));
}

// Select the current page on the left nav. Note: if already rendered, this
// will not effect any nodes.
function selectCurrentPageOnLeftNav() {
  function finalPathPart(str) {
    var pathParts = str.split(/\//);
    var lastPart = pathParts[pathParts.length - 1];
    return lastPart.split(/\?/)[0];
  }

  var pageBase = finalPathPart(document.location.href);

  evalXPathFromId(".//li/a", "gc-toc").select(function(node) {
    if (pageBase == finalPathPart(node.href)) {
      var parent = node.parentNode;
      if (node.firstChild.nodeName == 'DIV') {
        node.firstChild.className = "leftNavSelected";
      } else {
        parent.className = "leftNavSelected";
      }
      parent.removeChild(node);
      parent.insertBefore(node.firstChild, parent.firstChild);
      return true;
    }
  });
}

/*
 * Template Callout Functions
 * The jstProcess() will call out to these functions from within the page
 * template
 */

function stableAPIs() {
  return schema.filter(function(module) {
    return !module.nodoc && module.namespace.indexOf("experimental") < 0;
  }).map(function(module) {
    return module.namespace;
  }).sort();
}

function experimentalAPIs() {
  return schema.filter(function(module) {
    return !module.nodoc && module.namespace.indexOf("experimental") == 0;
  }).map(function(module) {
    return module.namespace;
  }).sort();
}

function webInspectorAPIs() {
  return schema.filter(function(module) {
    return !module.nodoc && module.namespace.indexOf("webInspector.") !== 0;
  }).map(function(module) {
    return module.namespace;
  }).sort();
}

function getDataFromPageHTML(id) {
  var node = document.getElementById(id);
  if (!node)
    return;
  return node.innerHTML;
}

function isArray(type) {
  return type.type == 'array';
}

function isFunction(type) {
  return type.type == 'function';
}

function getTypeRef(type) {
  return type["$ref"];
}

function getEnumValues(enumList, type) {
  if (type === "string") {
    enumList = enumList.map(function(e) { return '"' + e + '"'});
  }
  var retval = enumList.join(', ');
  return "[" + retval + "]";
}

function showPageTOC() {
  return module || getDataFromPageHTML('pageData-showTOC');
}

function showSideNav() {
  return getDataFromPageHTML("pageData-showSideNav") != "false";
}

function getStaticTOC() {
  var staticHNodes = evalXPathFromId(".//h2|h3", "static");
  var retval = [];
  var lastH2;

  staticHNodes.forEach(function(n, i) {
    var anchorName = n.id || n.nodeName + "-" + i;
    if (!n.id) {
      var a = document.createElement('a');
      a.name = anchorName;
      n.parentNode.insertBefore(a, n);
    }
    var dataNode = { name: n.innerHTML, href: anchorName };

    if (n.nodeName == "H2") {
      retval.push(dataNode);
      lastH2 = dataNode;
      lastH2.children = [];
    } else {
      lastH2.children.push(dataNode);
    }
  });

  return retval;
}

// This function looks in the description for strings of the form
// "$ref:TYPE_ID" (where TYPE_ID is something like "Tab" or "HistoryItem") and
// substitutes a link to the documentation for that type.
function substituteTypeRefs(description) {
  var regexp = /\$ref\:\w+/g;
  var matches = description.match(regexp);
  if (!matches) {
    return description;
  }
  var result = description;
  for (var i = 0; i < matches.length; i++) {
    var type = matches[i].split(":")[1];
    var page = null;
    try {
      page = getTypeRefPage({"$ref": type});
    } catch (error) {
      console.log("substituteTypeRefs couldn't find page for type " + type);
      continue;
    }
    var replacement = "<a href='" + page + "#type-" + type + "'>" + type +
                      "</a>";
    result = result.replace(matches[i], replacement);
  }

  return result;
}

function getTypeRefPage(type) {
  return typeModule[type.$ref].namespace + ".html";
}

function getPageName() {
  var pageDataName = getDataFromPageHTML("pageData-name");
  // Allow empty string to be explitly set via pageData.
  if (pageDataName == "") {
    return pageDataName;
  }

  return pageDataName || apiModuleName || pageName;
}

function getPageTitle() {
  var pageName = getPageName();
  var pageTitleSuffix = "Google Chrome Extensions - Google Code";
  if (pageName == "") {
    return pageTitleSuffix;
  }

  return pageName + " - " + pageTitleSuffix;
}

function getModuleName() {
  return API_MODULE_PREFIX + module.namespace;
}

function getFullyQualifiedFunctionName(scope, func) {
  return (getObjectName(scope) || getModuleName()) + "." + func.name;
}

function getObjectName(typeName) {
  return typeName.charAt(0).toLowerCase() + typeName.substring(1);
}

function isExperimentalAPIPage() {
  return (getPageName().indexOf('.experimental.') >= 0 &&
          getPageName().indexOf('.experimental.*') < 0);
}

function hasCallback(parameters) {
  return (parameters.length > 0 &&
          parameters[parameters.length - 1].type == "function");
}

function getCallbackParameters(parameters) {
  return parameters[parameters.length - 1];
}

function getAnchorName(type, name, scope) {
  return type + "-" + (scope ? scope + "-" : "") + name;
}

function shouldExpandObject(object) {
  return (object.type == "object" && object.properties);
}

function getPropertyListFromObject(object) {
  var propertyList = [];
  for (var p in object.properties) {
    var prop = object.properties[p];
    prop.name = p;
    propertyList.push(prop);
  }
  return propertyList;
}

function getTypeName(schema) {
  if (schema.$ref)
    return schema.$ref;

  if (schema.choices) {
    var typeNames = [];
    schema.choices.forEach(function(c) {
      typeNames.push(getTypeName(c));
    });

    return typeNames.join(" or ");
  }

  if (schema.type == "array")
    return "array of " + getTypeName(schema.items);

  if (schema.isInstanceOf)
    return schema.isInstanceOf;

  return schema.type;
}

function getSignatureString(parameters) {
  if (!parameters)
    return "";
  var retval = [];
  parameters.forEach(function(param, i) {
    retval.push(getTypeName(param) + " " + param.name);
  });

  return retval.join(", ");
}

function sortByName(a, b) {
  if (a.name < b.name) {
    return -1;
  }
  if (a.name > b.name) {
    return 1;
  }
  return 0;
}
