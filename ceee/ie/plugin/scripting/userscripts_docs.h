// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CEEE_IE_PLUGIN_SCRIPTING_USERSCRIPTS_DOCS_H_  // Mainly for lint
#define CEEE_IE_PLUGIN_SCRIPTING_USERSCRIPTS_DOCS_H_

/** @page UserScriptsDoc Detailed documentation of the UserScriptsLibrarian.

@section UserScriptsLibrarianIntro Introduction

We need to be able to load the user scripts information (including url pattern
matching info) from an extension manifest file and make it available for
insertion in a WEB page as needed.

@section IE

We already have a class (ExtensionManifest) that reads information from a
manifest file that was first needed to load the toolstrip info. So we need to
add code in there to load the information related to user scripts.

Since this class has a one to one relationship with a manifest file, we need
another one to hold on all of the scripts from all the extensions that we will
eventually read from and make them accessible to the page API to load the ones
that match the current URL.

So this other class (UserScriptsLibrarian) has a method to add UserScripts and
to retrieve the CSS and JavaScript content of the ones that match a given URL.
It reuses the UserScript object.

TODO(siggi@chromium.org): Unbranch this code; a lot of it was taken
from (and adapted) the UserScriptSlave class which is used to apply
user scripts to a given WebKit frame.

Our version simply returns the JavaScript code and CSS as text and let the
PageApi class take care of the insertion in the browser script engine.

The PageAPI code must react to the earliest event after the creation of the
HTML document so that we can inject the CSS content before the page is rendered.
For the user script, we have not yet found a way to inject the scripts that
specify the start location.

@section Firefox

TODO: The implementation has changed, so we should update this doc.

@section IEFF For both IE and Firefox

The JavaScript code returned by the functions extracting them from the user
scripts contain the proper heading to emulate the Greasemonkey API
(taken from greasemonkey_api.js) for running scripts marked as stand alone
(i.e., an extension without a public key), and it also wraps them
individually within an anonymous function.

The code taking care of injecting the code in chromium (in UserScriptSlave)
concatenate all the scripts of a dictionary entry in the
manifest file, and executes them in a separate context.
@note However, this is being changed: http://crbug.com/22110.

We don't have this capacity yet but the code must be written in a way that it
will be easy to add this later on, so the methods returning the script code
should only concatenate together the scripts of a single dictionary entry at a
time. The CSS styles can all be bundled up in a single string though.
**/

#endif  // CEEE_IE_PLUGIN_SCRIPTING_USERSCRIPTS_DOCS_H_
