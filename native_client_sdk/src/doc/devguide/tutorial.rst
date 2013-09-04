.. _tutorial:

#############################
C++ Tutorial: Getting Started
#############################

.. contents::
  :local:
  :backlinks: none
  :depth: 2

Overview
========

This tutorial shows you how to create, compile, and run a Native Client web
application. The Native Client module you will create as part of the web
application will be written in C++.

We recommend reading the :doc:`Native Client Technical Overview
<../overview>` prior to going through this tutorial.

Parts in a Native Client application
------------------------------------

A Native Client web application consists of at least three parts:
**TODO(binji)**: This is duplicated in the technical overview. Make sure it is
consistent in each.

* A **web page** (*\*.html*)

  The web page can include HTML, JavaScript, and CSS (the JavaScript and CSS
  can also go in separate .js and .css files).

* A **Native Client module** (*\*.c* or *\*.cc* before compiling; *\*.nexe*
  after compiling)

  Native Client modules can be written in C or C++. Modules use the Pepper API,
  included in the SDK, as a bridge between the browser and the modules.

* A **Manifest** file (*\*.nmf*)

  Browsers use an application's manifest file to determine which compiled Native
  Client module to load based on the instruction set architecture of the user's
  machine (e.g., x86-32, x86-64, or ARM).

What the application in this tutorial does
------------------------------------------

The application in this tutorial shows how to load a Native Client module in a
web page, and how to send messages between JavaScript code and the C or C++
code in the Native Client module. In this simple application, the JavaScript
code in the web page sends a 'hello' message to the Native Client module. When
the Native Client module receives a message, it checks whether the message is
equal to the string 'hello'. If it is, the Native Client module returns a
message saying 'hello from NaCl'. A JavaScript alert panel displays the message
received from the Native Client module.

This tutorial also shows you how to create a set of template files that you can
use as a starting point for a Native Client application. The template code sets
up a simple message handler on the Native Client side, and includes boilerplate
code in the HTML file for adding an event listener to the web page to receive
messages from the Native Client module.

Communication between JavaScript code and Native Client modules
---------------------------------------------------------------

Communication between JavaScript code in the browser and C or C++ code in a
Native Client module is two-way: JavaScript code can send messages to the
Native Client module; the C or C++ code can respond to messages from
JavaScript, or it can initiate its own messages to JavaScript. In all cases,
the communication is asynchronous: The caller (the JavaScript code in the
browser or the C/C++ code in the Native Client module) sends a message, but the
caller does not wait for, or may not even expect, a response. This behavior is
analogous to client/server communication on the web, where the client posts a
message to the server and returns immediately. The Native Client messaging
system is part of the Pepper API, and is described in detail in the
:doc:`Messaging System <coding/message-system>` chapter in the Developer's
Guide.

Step 1: Download and install the Native Client SDK
==================================================

Follow the instructions on the :doc:`Download <../sdk/download>` page to
download and install the Native Client SDK.

.. Note::
  :class: caution

  **Important:** A number of tools in the SDK require Python to run. Python is
  typically included on Mac and Linux systems, but not on Windows systems. To
  check whether you have Python installed on your system, enter the
  '``python``' command on the command line; you should get the interactive
  Python prompt (``>>>``). On Mac systems, you also need to install '``make``'
  in order to build and run the examples in the SDK; one easy way to get
  '``make``', along with several other useful tools, is to install Xcode
  Developer Tools. Follow the instructions at the top of the :doc:`Download
  <../sdk/download>` page if you need to install Python and/or Xcode
  Developer Tools.

Step 2: Start a local server
============================

TODO(binji): This is not necessary anymore; we can use ``make run``. Some of
the information about why you need a webserver is still useful though...
Remove?

To protect against security vulnerabilities, you must load Native Client
modules from a web server (either remote or local). **Simply dragging and
dropping Native Client files into the browser address bar will not work.** For
more information, read about the `Same Origin Policy
<http://www.w3.org/Security/wiki/Same_Origin_Policy>`_, which protects the
user's file system from outside access.

The Native Client SDK includes a simple Python web server that you can use to
run applications that you build (including the application in this tutorial).
The server is located in the tools directory. To start the web server, go to
the examples directory in the SDK bundle that you are using and run the
``httpd.py`` script. For example, if you are using the ``pepper_28`` bundle,
run the following commands:

.. naclcode::
  :prettyprint: 0

  cd pepper_28/examples
  python ../tools/httpd.py

If you don't specify a port number, the server defaults to port 5103, and you
can access the server at http://localhost:5103.

Of course, you don't have to use the server included in the SDK---any web server
will do. If you prefer to use another web server already installed on your
system, that's fine. Note also that there are ways to run Native Client
applications during development without a server, but these techniques require
you to create additional files for your application (see :doc:`Running Native
Client Applications <devcycle/running>` for details). For this tutorial,
your application must come from a server.

.. _step_3:

Step 3: Set up Google Chrome
============================

Set up the Chrome browser as follows:

a. Make sure you are using the minimum required version of Chrome.

   * Your version of Chrome must be equal to or greater than the version of your
     Pepper bundle. For example, if you're developing with the ``pepper_28``
     bundle, you must use Google Chrome version 28 or greater. To find out what
     version of Chrome you're using, type ``about:chrome`` or ``about:version``
     in the Chrome address bar.

b. Enable the Native Client flag in Chrome. (Native Client is enabled by
   default for applications distributed through the Chrome Web Store. To run
   Native Client applications that are not distributed through the Chrome Web
   Store, e.g., applications that you build and run locally, you must
   specifically enable the Native Client flag in Chrome.)

   * Type ``about:flags`` in the Chrome address bar and scroll down to "Native
     Client".
   * If the link below "Native Client" says "Disable", then Native Client is
     already enabled and you don't need to do anything else.
   * If the link below "Native Client" says "Enable", click the "Enable" link,
     scroll down to the bottom of the page, and click the "Relaunch Now" button.
     All browser windows will restart when you relaunch Chrome.

c. Disable the Chrome cache. (Chrome caches resources aggressively; you should
   disable the cache whenever you are developing a Native Client application in
   order to make sure Chrome loads new versions of your application.)

   * Open Chrome's developer tools by clicking the menu icon |menu-icon| and
     choosing Tools > Developer tools.
   * Click the gear icon |gear-icon| in the bottom right corner of the Chrome
     window.
   * Under the "General" settings, check the box next to "Disable cache".

.. |menu-icon| image:: /images/menu-icon.png
.. |gear-icon| image:: /images/gear-icon.png

Step 4: Create a set of stub files for your application
=======================================================

Create a set of stub files as follows:

a. Download `hello_tutorial.zip
   <https://developers.google.com/native-client/devguide/hello_tutorial.zip>`_.

b. Unzip hello_tutorial.zip:

   * On Mac/Linux, run the command "``unzip hello_tutorial.zip``" in a Terminal
     window.
   * On Windows, right-click on the .zip file and select "Extract All..." A
     dialog box will open; enter a location and click "Extract".

c. Unzipping hello_tutorial.zip creates a directory called ``hello_tutorial``
   with the following files:

   * ``hello_tutorial.html``
   * ``hello_tutorial.cc``
   * ``hello_tutorial.nmf``
   * ``Makefile``
   * ``make.bat`` (for Windows)

d. Move the ``hello_tutorial`` directory so that it's under the ``examples``
   directory where you started the local server. Its location should be, e.g.,
   ``pepper_28/examples/hello_tutorial``.

   * On Windows, depending on the location you entered when you unzipped the
     file, there may be two ``hello_tutorial`` directories—one nested within
     the other. Move only the inner (nested) directory to the ``examples``
     directory.

.. Note::
  :class: note

  **Note regarding the location of project directories:**

  * In this tutorial, you are adding the ``hello_tutorial`` directory under the
    ``examples`` directory because the ``examples`` directory is where your
    local server is running, ready to serve your tutorial application. You can
    place your project directory anywhere on your file system, as long as that
    location is being served by your server.
  * If you do place the ``hello_tutorial`` project directory in another
    location, you must set the `environment variable
    <http://en.wikipedia.org/wiki/Environment_variable>`_ ``NACL_SDK_ROOT`` to
    point to the top-level directory of the bundle you are using (e.g.,
    ``<location-where-you-installed-the-SDK>/pepper_28``) in order for the
    Makefile that's included in the project directory to work.
  * If you use the location recommended above
    (``pepper_28/examples/hello_tutorial``), be careful when you update the
    SDK.  The command '``naclsdk update pepper_28 --force``' will overwrite the
    ``pepper_28`` directory, so move any project directories you want to keep
    to another location.

Step 5: Compile the Native Client module and run the stub application
=====================================================================

The files you downloaded in the previous step constitute a stub application
that simply loads a Native Client module into a web page and updates a
``<div>`` element on the page with the status of the module load.

To compile the Native Client module ``hello_tutorial.cc,`` run '``make``':

.. naclcode::
  :prettyprint: 0

  cd pepper_28/examples/hello_tutorial
  make

The '``make``' command runs the necessary compile and link commands to produce
three executable Native Client modules (for the x86-32, x86-64, and ARM
architectures). The executable files are named as follows:

* ``hello_tutorial_x86_32.nexe``
* ``hello_tutorial_x86_64.nexe``
* ``hello_tutorial_arm.nexe``

Assuming you are using the local server and the project directory specified
above, you can load the ``hello_tutorial.html`` web page into Chrome by visiting
the following URL: http://localhost:5103/hello_tutorial/hello_tutorial.html. If
Chrome loads the Native Client module successfully, the Status display on the
page should change from "LOADING..." to "SUCCESS".

Step 6: Review the code in the stub application
===============================================

The section highlights some of the code in the stub application.

Makefile
  ``Makefile`` contains the compile and link commands to build the executable
  Native Client modules (.nexe files) for your application. The Native Client
  SDK includes multiple GCC‑based toolchains to build modules for multiple
  architectures (x86 and ARM) using different implementations of the C library
  (`newlib <http://www.sourceware.org/newlib/>`_ and `glibc
  <http://www.gnu.org/software/libc/>`_). The commands in the tutorial
  ``Makefile`` build the application using the newlib C library for the x86 and
  ARM architectures. The commands use the toolchains located in the
  ``pepper_28/toolchain/<platform>_x86_newlib`` and ``<platform>_arm_newlib``
  directories. For information about how to use Makefiles and the '``make``'
  command, see the `GNU 'make' manual
  <http://www.gnu.org/software/make/manual/make.html>`_.

hello_tutorial.nmf
  ``hello_tutorial.nmf`` is a Native Client manifest file that tells Chrome
  which compiled Native Client module (.nexe) to load based on the instruction
  set architecture of the user's machine (e.g., x86-32, x86-64, or ARM). For
  applications compiled using glibc, manifest files must also specify the
  shared libraries that the applications use.

hello_tutorial.html
  ``hello_tutorial.html`` is the web page that corresponds to your application.
  The page includes an ``<embed>`` element that loads the compiled Native
  Client module:

  .. naclcode::

    <div id="listener">
      <script type="text/javascript">
        var listener = document.getElementById('listener');
        listener.addEventListener('load', moduleDidLoad, true);
        listener.addEventListener('message', handleMessage, true);
      </script>

      <embed name="nacl_module"
         id="hello_tutorial"
         width=0 height=0
         src="hello_tutorial.nmf"
         type="application/x-nacl" />
    </div>

  The ``src`` attribute in the ``<embed>`` element points to the Native Client
  manifest file, which tells the browser which .nexe file to load based on the
  instruction set architecture of the user's machine. The ``width`` and
  ``height`` attributes in the ``<embed>`` element are set to 0 because the
  Native Client module in this example does not have any graphical component.
  The ``type`` attribute declares the MIME type to be ``x-nacl``, i.e., an
  executable Native Client module.

  The ``<embed>`` element is wrapped inside a ``<div>`` element that has two
  event listeners attached—one for the 'load' event, which fires when the
  browser successfully loads the Native Client module, and one for the
  'message' event, which fires when the Native Client module uses the
  ``PostMessage()`` method (in the `pp::Instance
  <https://developers.google.com/native-client/peppercpp/classpp_1_1_instance>`_
  class) to send a message to the JavaScript code in the application. This
  technique of attaching the event listeners to a parent ``<div>`` element
  (rather than directly to the ``<embed>`` element) is used to ensure that the
  event listeners are active before the module 'load' event fires.

  The simple event handlers in this tutorial are implemented in the
  ``moduleDidLoad()`` and ``handleMessage()`` JavaScript functions.
  ``moduleDidLoad()`` changes the text inside the 'status_field' ``<div>``
  element. handleMessage() displays the content of messages sent from the
  Native Client module in a browser alert panel. For a description of 'load',
  'message', and other Native Client events, see the :doc:`Progress Events
  <coding/progress-events>` chapter of the Developer's Guide.

hello_tutorial.cc
  Native Client includes the concept of modules and instances:

  * A **module** is C or C++ code compiled into an executable .nexe file.
  * An **instance** is a rectangle on a web page that is managed by a module.
    The rectangle can have dimensions 0x0, in which case the instance does not
    have a visual component on the web page. An instance is created by
    including an ``<embed>`` element in a web page. A module may be included in
    a web page multiple times by using multiple ``<embed>`` elements that refer
    to the module; in this case the Native Client runtime system loads the
    module once and creates multiple instances that are managed by the module.

  The example in this tutorial includes one module
  (``hello_tutorial_x86_32.nexe``, ``hello_tutorial_x86_64.nexe``, or
  ``hello_tutorial_arm.nexe``, depending on the instruction set architecture of
  the user's machine), and one instance (one ``<embed>`` element that loads the
  module). The source code for the module is in the file ``hello_tutorial.cc``.
  This source code contains the minimum code required in a C++ Native Client
  module—an implementation of the `Instance
  <https://developers.google.com/native-client/peppercpp/classpp_1_1_instance>`_
  and `Module
  <https://developers.google.com/native-client/peppercpp/classpp_1_1_module>`_
  classes. These implementations don't actually do anything yet.

Step 7: Modify the web page to send a message to the Native Client module
=========================================================================

In this step, you'll modify the web page (``hello_tutorial.html``) to send a
message to the Native Client module after the page loads the module.

Look for the JavaScript function ``moduleDidLoad()``, and add the new code below
(indicated by boldface type) to send a 'hello' message to the Native Client
module:

..naclcode::

  function moduleDidLoad() {
        HelloTutorialModule = document.getElementById('hello_tutorial');
        updateStatus('SUCCESS');
        // Send a message to the NaCl module.
        HelloTutorialModule.postMessage('hello');
  }

Step 8: Implement a message handler in the Native Client module
===============================================================

In this step, you'll modify the Native Client module (``hello_tutorial.cc``) to
respond to the message received from the JavaScript code in the application.
Specifically, you'll:

* implement the ``HandleMessage()`` function for the module, and
* use the ``PostMessage()`` function to send a message from the module to the
  JavaScript code

First, add code to define the variables used by the Native Client module (the
'hello' string you're expecting to receive from JavaScript and the reply string
you want to return to JavaScript as a response). In the file
``hello_tutorial.cc``, add this code after the ``#include`` statements:

.. naclcode::

  namespace {
  // The expected string sent by the browser.
  const char* const kHelloString = "hello";
  // The string sent back to the browser upon receipt of a message
  // containing "hello".
  const char* const kReplyString = "hello from NaCl";
  } // namespace

Now, implement the ``HandleMessage()`` method to check for ``kHelloString`` and
return ``kReplyString.`` Look for the following line:

.. naclcode::

    // TODO(sdk_user): 1. Make this function handle the incoming message.

Replace the above line with the boldface code below:

.. naclcode::

  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_string())
      return;
    std::string message = var_message.AsString();
    pp::Var var_reply;
    if (message == kHelloString) {
      var_reply = pp::Var(kReplyString);
      PostMessage(var_reply);
    }
  }

See the Pepper API documentation for additional information about the
`pp::Instance.HandleMessage
<https://developers.google.com/native-client/peppercpp/classpp_1_1_instance.html#a5dce8c8b36b1df7cfcc12e42397a35e8>`_
and `pp::Instance.PostMessage
<https://developers.google.com/native-client/peppercpp/classpp_1_1_instance.html#a67e888a4e4e23effe7a09625e73ecae9>`_
methods.

Step 9: Compile the Native Client module and run the application again
======================================================================

Compile the Native Client module by running the '``make``' command again.

Run the application by reloading hello_tutorial.html in Chrome. (The page
should be at http://localhost:5103/hello_tutorial/hello_tutorial.html assuming
the setup described above.)

After Chrome loads the Native Client module, you should see an alert panel
appear with the message sent from the module.

Troubleshooting
===============

If your application doesn't run, see :ref:`Step 3 <step_3>` above
to verify that you've set up your environment correctly, including both the
Chrome browser and the local server. Make sure that you're running a version of
Chrome that is equal to or greater than the SDK bundle version you are using,
that you've enabled the Native Client flag and relaunched Chrome, that you've
disabled the Chrome cache, and that **you're accessing your application from a
local web server (rather than by dragging the HTML file into your browser)**.

For additional troubleshooting information, check the `FAQ
<https://developers.google.com/native-client/faq.html#HangOnLoad>`_.

Next steps
==========

* See the :doc:`Application Structure <coding/application-structure>`
  chapter in the Developer's Guide for information about how to structure a
  Native Client module.
* Check the `C++ Reference
  <https://developers.google.com/native-client/peppercpp>`_ for details about
  how to use the Pepper APIs.
* Browse through the source code of the SDK examples (in the ``examples``
  directory) to learn additional techniques for writing Native Client
  applications and using the Pepper APIs.
* See the :doc:`Building <devcycle/building>`, :doc:`Running
  <devcycle/running>`, and :doc:`Debugging pages <devcycle/debugging>`
  for information about how to build, run, and debug Native Client
  applications.
* Check the `naclports <http://code.google.com/p/naclports/>`_ project to see
  what libraries have been ported for use with Native Client. If you port an
  open-source library for your own use, we recommend adding it to naclports
  (see `How to check code into naclports
  <http://code.google.com/p/naclports/wiki/HowTo_Checkin>`_).
