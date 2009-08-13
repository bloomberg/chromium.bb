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
var SCHEMA = "../api/extension_api.json";
var REQUEST_TIMEOUT = 2000;

function staticResource(name) { return "static/" + name + ".html"; }

// Base name of this page. (i.e. "tabs", "overview", etc...).
var pageBase;

// The name of this page that will be shown on the page in various places.
var pageName;

// Data to feed as context into the template.
var pageData = null;

// Schema Types (Referenced via $ref).
var types = {};

Array.prototype.each = function(f) {
  for (var i = 0; i < this.length; i++) {
    f(this[i], i);
  }
}

/*
 * Assigns all keys & values of |obj2| to |obj1|.
 */
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
  var pathParts = document.location.href.split(/\/|\./);
  pageBase = pathParts[pathParts.length - 2];
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
  fetchContent(SCHEMA, renderTemplate, function(error) {
    alert("Failed to load " + SCHEMA);
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
    if (onError)
      onError(error);
    else
      console.error("XHR Failed fetching: " + localUrl + "..." + error);
  }

  try {
    xhr.onreadystatechange = function(){
      if (xhr.readyState == 4) {
        if (xhr.responseText) {
          window.clearTimeout(abortTimerId);
          onSuccess(xhr.responseText);
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

/**
 * Parses the content in |schemaContent| to json, find appropriate api, adds any
 * additional required values, renders to html via JSTemplate, and unhides the
 * <body>.
 * This uses the root <html> element (the entire document) as the template.
 */
function renderTemplate(schemaContent) {
  pageData = {};
  var schema = JSON.parse(schemaContent);

  // Find all defined types
  schema.each(function(module) {
    if (module.types) {
      module.types.each(function(t) {
        types[t.id] = t;
      });
    }
  });

  schema.each(function(module) {
    if (module.namespace == pageBase) {
      // This page is an api page. Setup types and apiDefinition.
      pageData.apiDefinition = module;
      preprocessApi(pageData, schema);
    }
  });

  setupPageData(pageData, schema);

  // Render to template
  var input = new JsEvalContext(pageData);
  var output = document.getElementsByTagName("html")[0];
  jstProcess(input, output);

  selectCurrentPageOnLeftNav();
  
  // Show.
  var elm = document.getElementById("hider");
  elm.parentNode.removeChild(elm);

  if (parent && parent.done)
    parent.done();
}

function serializePage() {
 var s = new XMLSerializer();
 return s.serializeToString(document);
}

// Select the current page on the left nav. Note: if already rendered, this 
// will not effect any nodes.
function selectCurrentPageOnLeftNav() {
  var pathParts = document.location.href.split(/\//);
  var pageBase = pathParts[pathParts.length - 1];
  var leftNav = document.getElementById("leftNav");
  var results = document.evaluate('.//li/a', leftNav, null,
      XPathResult.UNORDERED_NODE_ITERATOR_TYPE, null);
  while(node = results.iterateNext()) {
    if (node.href.match(pageBase + "$")) {
      var parent = node.parentNode;
      parent.className = "leftNavSelected";
      parent.removeChild(node);
      parent.appendChild(node.firstChild);
      break;
    }
  }
}

function setupPageData(pageData, schema) {
  // Add a list of modules for the master TOC.
  pageData.apiModules = [];
  schema.each(function(s) {
    var m = {};
    m.module = s.namespace;
    m.name = s.namespace.substring(0, 1).toUpperCase() +
             s.namespace.substring(1);
    pageData.apiModules.push(m);
  });
  pageData.apiModules.sort(function(a, b) { return a.name > b.name; });
  
  // Set the page title (in order of preference).
  pageData.pageTitle = getDataFromPageHTML("pageData-title") || 
                       pageData.pageTitle || 
                       pageName;
}

function getDataFromPageHTML(id) {
  var node = document.getElementById(id);
  if (!node)
    return;
  return node.innerHTML;
}

/**
 * Augment the |module| with additional values that are required by the
 * template. |schema| is the full schema (including all modules).
 */
function preprocessApi(pageData, schema) {
  var module = pageData.apiDefinition;
  pageData.pageTitle = "chrome." + module.namespace + " API Reference";
  pageData.h1Header = "chrome." + module.namespace
  module.functions.each(function(f) {
    f.fullName = "chrome." + module.namespace + "." + f.name;
    linkTypeReferences(f.parameters);
    assignTypeNames(f.parameters);

    // Look for a callback that defines parameters.
    if (f.parameters.length > 0) {
      var lastParam = f.parameters[f.parameters.length - 1];
      if (lastParam.type == "function" && lastParam.parameters) {
        linkTypeReferences(lastParam.parameters);
        assignTypeNames(lastParam.parameters);        
        f.callbackParameters = lastParam.parameters;
        f.callbackSignature = generateSignatureString(lastParam.parameters);
        f.callbackParameters.each(function(p) {
          addPropertyListIfObject(p);
        });
      }
    }

    // Setup any type: "object" pameters to have an array of params (rather than 
    // named properties).
    f.parameters.each(function(param) {
      addPropertyListIfObject(param);
    });

    // Setup return typeName & _propertyList, if any.
    if (f.returns) {
      linkTypeReference(f.returns);
      f.returns.typeName = typeName(f.returns);
      addPropertyListIfObject(f.returns);
    }
  });

  module.events.each(function(e) {
    linkTypeReferences(e.parameters);
    assignTypeNames(e.parameters);    
    e.callSignature = generateSignatureString(e.parameters);
    e.parameters.each(function(p) {
      addPropertyListIfObject(p);
    });
  });
}

/*
 * For function, callback and event parameters, we want to document the
 * properties of any "object" type. This takes an param, and if it is an
 * "object" type, creates a "_parameterList" property in the form as
 * |parameters| list used by the template.
 */
function addPropertyListIfObject(object) {
  if (object.type != "object" || !object.properties)
    return;

  var propertyList = [];
  for (var p in object.properties) {
    var prop = object.properties[p];
    prop.name = p;
    propertyList.push(prop);
  }
  assignTypeNames(propertyList);
  object._propertyList = propertyList;
}

function linkTypeReferences(parameters) {
  parameters.each(function(p) {
    linkTypeReference(p, types);
  });
}

function linkTypeReference(schema) {
  if (schema.$ref)
    extend(schema, types[schema.$ref]);
}

/**
 * Assigns a typeName(param) to each of the |parameters|.
 */
function assignTypeNames(parameters) {
  parameters.each(function(p) {
    p.typeName = typeName(p);
  });
}

/**
 * Generates a short text summary of the |schema| type
 */
function typeName(schema) {
  if (schema.$ref)
    schema = types[schema.$ref];

  if (schema.choices) {
    var typeNames = [];
    schema.choices.each(function(c) {
      typeNames.push(typeName(c));
    });

    return typeNames.join(" or ");
  }

  if (schema.type == "array")
    return "array of " + typeName(schema.items);

  return schema.type;
}

/** 
 * Generates a simple string representation of the signature of a function
 * whose |parameters| are json schemas.
 */
function generateSignatureString(parameters) {
  var retval = [];
  parameters.each(function(param, i) {
    retval.push(param.typeName + " " + param.name);
  });

  return retval.join(", ");	
}