// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * External functions for MathJax bridge.
 * @typedef {Object}
 */
function MathJax() {}

/**
 * @type {Function}
 */
MathJax.Callback;

/**
 * @param {Array<*>} args
 * @param {string} err
 */
MathJax.Callback.After = function(args, err) { };


/**
 * @typedef {{root: MathJax.RootElement,
 *  inputID: string}}
 */
MathJax.Jax;


/**
 * @typedef {{inputID: string,
 *  spanID: number,
 *  data: Array<Object>,
 *  id: string,
 *  texClass: string}}
 */
MathJax.RootElement;
MathJax.RootElement.prototype;

/**
 * @param {string} name
 */
MathJax.RootElement.prototype.toMathML = function(name) { };

/**
 */
MathJax.RootElement.prototype.toMathMLattributes = function() { };


/**
 * @typedef {Object}
 */
MathJax.Hub;

/**
 * @param {string} id
 */
MathJax.Hub.getJaxFor = function(id) { };

/**
 */
MathJax.Hub.getAllJax = function() { };

/**
 * @type {{PreProcessor: Function,
 *  MessageHook: function(string, function(Array<string>)):
 *                        function(Array<string>),
 *  StartupHook: function(string, function(Array<string>)):
 *                        function(Array<string>),
 *  LoadHook: function(string, function(Array<string>)):
 *                        function(Array<string>)}}
 */
MathJax.Hub.Register;


/**
 * @typedef {Object}
 */
MathJax.OutputJax;


/**
 * @typedef {Object}
 */
MathJax.ElementJax;
MathJax.ElementJax.prototype.mml;

/**
 * @param {?string} mml MathML expression.
 */
MathJax.ElementJax.mml = function(mml) { };


/**
 * @type {MathJax.RootElement}
 */
MathJax.ElementJax.mml.mbase;
MathJax.ElementJax.mml.mbase.prototype;


/**
 * @type {MathJax.RootElement}
 */
MathJax.ElementJax.mml.mfenced;
MathJax.ElementJax.mml.mfenced.prototype;


/**
 * @param {string} err
 */
MathJax.ElementJax.mml.merror = function(err) { };


/**
 * @type {{DOUBLESTRUCK: string,
 *         NORMAL: string}}
 */
MathJax.ElementJax.mml.VARIANT;


/**
 * @type {{OP: number}}
 */
MathJax.ElementJax.mml.TEXCLASS;


/**
 * @typedef {Object}
 */
MathJax.InputJax;


/**
 * @typedef {Object}
 */
MathJax.HTML;


/**
 * Creates an HTML element from a node tag, an object with attributes and an
 * array of text content.
 * @param {string} tag
 * @param {Object<string>} attribs
 * @param {Array<string>} text
 */
MathJax.HTML.Element = function(tag, attribs, text) { };


/**
 * @type {{Parse: function(string): MathJax.ElementJax,
 * postfilterHooks: {Execute:
 *   function({math: MathJax.RootElement, display: boolean, script: Element})},
 * prefilterHooks: {Execute:
 *   function({math: string, display: boolean, script: Element})},
 * Definitions: Object}}
 */
MathJax.InputJax.TeX;


/**
 * MediaWiki object.
 * @typedef {Object}
 */
function mediaWiki() {}


/**
 * This is the definition of the type that's returned from the PDF plug-in.
 * @constructor
 */
var PDFAccessibilityJSONReply = function() {};

/**
 * Whether the PDF has finished loading or not.
 * @type {boolean}
 */
PDFAccessibilityJSONReply.prototype.loaded;

/**
 * Whether the PDF allows accessible text access. Unfortunately PDFs can
 * mark themselves as not copyable even for accessibility.
 * @type {boolean}
 */
PDFAccessibilityJSONReply.prototype.copyable;

/**
 * The number of pages in the PDF.
 * @type {number}
 */
PDFAccessibilityJSONReply.prototype.numberOfPages;

/**
 * The height of each PDF page in points.
 * @type {number}
 */
PDFAccessibilityJSONReply.prototype.height;

/**
 * The width of each PDF page in points.
 * @type {number}
 */
PDFAccessibilityJSONReply.prototype.width;

/**
 * The text boxes in the PDF, this is where most of the content is returned.
 * Each text box has a bounding box (left, top, width, height) and
 * each of these contains an array of nodes of type 'text' or 'url'.
 * @type {Array<
 *           {left: number, top: number, width: number, height: number,
 *            textNodes: Array<
 *                {type: string, text: string, url: string}>
 *           }>
 *       }
 */
PDFAccessibilityJSONReply.prototype.textBox;
