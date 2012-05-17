This file describes steps and files needed when adding a new API to Chrome.
Before you start coding your new API, though, make sure you follow the process
described at:
  http://www.chromium.org/developers/design-documents/extensions/proposed-changes/apis-under-development

Two approaches are available for writing your API specification. The original
approach relies on JSON specification files. The more recent and simpler system 
uses Web IDL files, but does not yet support all the features of the JSON files.
Discuss with a member of the extensions team (aa@chromium.org) before you decide
which approach is better suited to your API.

The following steps suppose you're writing an experimental API called "Foo".

--------------------------------------------------------------------------------
APPROACH 1: JSON FILES

1) Write your API specification.
Create "chrome/common/extensions/api/experimental_foo.json". For inspiration 
look at the "app" API. Include descriptions fields to generate the
documentation.

2) Add your API specification to the project.
Add an "<include ...>" line with your JSON specification file to
"chrome/common/extensions_api_resources.grd".

3) Write the API function handlers.
Create foo_api.cc and foo_api.h under "chrome/browser/extensions/api/foo". You
should use the JSON Schema Compiler. Look at the "alarms_api.cc" for details on
how to do that.

4) Register function handlers.
In "chrome/browser/extensions/extension_function_registry.cc" include foo_api.h 
and instantiate a RegisterFunction for each function you created in (3).

--------------------------------------------------------------------------------
APPROACH 2: IDL FILES

1) Write your API specification.
Create "chrome/common/extensions/api/experimental_foo.idl". For inspiration look
at "alarms.idl". Include comments, they will be used to automatically generate
the documentation.

2) Add your API specification to the project.
Add "experimental_foo.idl" to the "idl_schema_files" section in
"chrome/common/extensions/api/api.gyp".

3) Write the API function handlers.
Create foo_api.cc and foo_api.h under "chrome/browser/extensions/api/foo". You
should use the JSON Schema Compiler. Look at the "alarms_api.cc" for details on
how to do that.

4) Nothing to do! Function handlers are automatically registered for you.

--------------------------------------------------------------------------------
STEPS COMMON TO BOTH APPROACHES

5) Write support classes for your API
If your API needs any support classes add them to
"chrome/browser/extensions/api/foo". Some old APIs added their support classes
directly to chrome/browser/extensions. Don't do that.

6) Update the project with your new files.
The files you created in (3) and (5) should be added to
"chrome/chrome_browser_extensions.gypi".

--------------------------------------------------------------------------------
GENERATING DOCUMENTATION

7) Build the project. (Only required if you used IDL files.)
If you used IDL files, you need to build the project once in order for the
documentation to be properly generated. Do this now. (This is required in order
to generate the JSON file used to generate documentation.)

8) Add your JSON file to the documentation controller
Open "chrome/common/extensions/docs/js/api_page_generator.js" and add a line
referring to "../api/experimental_foo.json". Do this even if you used the IDL
approach as this JSON file has been generated in (7).

9) Write the static HTML page.
Write a small snippet of static HTML describing your API in
"chrome/common/extensions/docs/static/experimental.foo.html". For the moment,
just include the following in this file, adjusting it to describe your API:

  <div id="pageData-name" class="pageData">Experimental Foo APIs</div>

  <!-- BEGIN AUTHORED CONTENT -->
  <p>The current methods allow applications to...</p>
  <!-- END AUTHORED CONTENT -->

10) Build the documentation.
You will need to build DumpRenderTree once before you can build the
documentation. Once this is done, from "chrome/common/extensions/docs" run
"build/build.py". For more information on building documentation see README.txt
in "chrome/common/extensions/docs".

--------------------------------------------------------------------------------
WRITING TESTS

TODO(beaudoin)

