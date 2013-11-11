.. _devcycle-debugging:

#########
Debugging
#########

This document describes tools and techniques you can use to debug, monitor,
and measure your application's performance.

.. contents:: Table Of Contents
  :local:
  :backlinks: none
  :depth: 2

Diagnostic information
======================

Viewing process statistics with the task manager
------------------------------------------------

You can use Chrome's Task Manager to display information about a Native Client
application:

#. Open the Task Manager by clicking the menu icon |menu-icon| and choosing
   **Tools > Task manager**.
#. When the Task Manager window appears, verify that the columns displaying
   memory information are visible. If they are not, right click in the header
   row and select the memory items from the popup menu that appears.

A browser window running a Native Client application will have at least two
processes associated with it: a process for the app's top level (the render
process managing the page including its HTML and any JavaScript) and one or
more processes for each instance of a Native Client module embedded in the page
(each process running native code from one nexe file). The top-level process
appears with the application's icon and begins with the text "App:". A Native
Client process appears with a Chrome extension icon (a jigsaw puzzle piece
|puzzle|) and begins with the text "Native Client module" followed by the URL
of its manifest file.

From the Task Manager you can view the changing memory allocations of all the
processes associated with a Native Client application. Each process has its own
memory footprint. You can also see the rendering rate displayed as frames per
second (FPS). Note that the computation of render frames can be performed in
any process, but the rendering itself is always done in the top level
application process, so look for the rendering rate there.

Controlling the level of Native Client error and warning messages
-----------------------------------------------------------------

Native Client prints warning and error messages to stdout and stderr. You can
increase the amount of Native Client's diagnostic output by setting the
following `environment variables
<http://en.wikipedia.org/wiki/Environment_variable>`_:

* NACL_DEBUG_ENABLE=1
* PPAPI_BROWSER_DEBUG=1
* NACL_PLUGIN_DEBUG=1
* NACL_PPAPI_PROXY_DEBUG=1
* NACL_SRPC_DEBUG=[1-255] (use a higher number for more verbose debug output)
* NACLVERBOSITY=[1-255]

Basic debugging
===============

Writing messages to the JavaScript console
------------------------------------------

You can send messages from your C/C++ code to JavaScript using the PostMessage
call in the :doc:`Pepper messaging system <../coding/message-system>`. When the
JavaScript code receives a message, its message event handler can call
`console.log() <https://developer.mozilla.org/en/DOM/console.log>`_ to write
the message to the JavaScript `console
<https://developers.google.com/chrome-developer-tools/docs/console>`_ in
Chrome's Developer Tools.

Debugging with printf
---------------------

Your C/C++ code can perform inline printf debugging to stdout and stderr by
calling fprintf() directly, or by using cover functions like these:

.. naclcode::

  #include <stdio.h>
  void logmsg(const char* pMsg){
    fprintf(stdout,"logmsg: %s\n",pMsg);
  }
  void errormsg(const char* pMsg){
    fprintf(stderr,"logerr: %s\n",pMsg);
  }

By default stdout and stderr will appear in Chrome's stdout and stderr stream
but they can also be redirected as described below.

Redirecting output to log files
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can redirect stdout and stderr to output files by setting these environment variables:

* ``NACL_EXE_STDOUT=c:\nacl_stdout.log``
* ``NACL_EXE_STDERR=c:\nacl_stderr.log``

There is another variable, ``NACLLOG``, that you can use to redirect Native
Client's internally-generated messages. This variable is set to stderr by
default; you can redirect these messages to an output file by setting the
variable as follows:

* ``NACLLOG=c:\nacl.log``

.. Note::
  :class: note

  **Note:** If you set the NACL_EXE_STDOUT, NACL_EXE_STDERR, or NACLLOG
  variables to redirect output to a file, you must run Chrome with the
  ``--no-sandbox`` flag.  You must also be careful that each variable points to
  a different file.

Redirecting output to the JavaScript console
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can also cause output from printf statements in your C/C++ code to be
relayed to the JavaScript side of your application through the Pepper messaging
system, where you can then write the output to the JavaScript console. Follow
these steps:

#. Set the NACL_EXE_STDOUT and NACL_EXE_STDERR environment variables as
   follows:

   * NACL_EXE_STDOUT=DEBUG_ONLY:dev://postmessage
   * NACL_EXE_STDERR=DEBUG_ONLY:dev://postmessage

   These settings tell Native Client to use PostMessage() to send output that
   your Native Client module writes to stdout and stderr to the JavaScript side
   of your application.

#. Register a JavaScript handler to receive messages from your Native Client
   module:

   .. naclcode::

     <div id="nacl_container">
       <script type="text/javascript">
         var container = document.getElementById('nacl_container');
         container.addEventListener('message', handleMessage, true);
       </script>
       <embed id="nacl_module"
              src="my_application.nmf"
              type="application/x-nacl" />
     </div>

#. Implement a simple JavaScript handler that logs the messages it receives to
   the JavaScript console:

   .. naclcode::

     function handleMessage(message_event) {
       console.log(message_event.data);
     }

   This handler works in the simple case where the only messages your Native
   Client module sends to JavaScript are messages with the output from stdout
   and stderr. If your Native Client module also sends other messages to
   JavaScript, your handler will need to be more complex.

   Once you've implemented a message handler and set up the environment
   variables as described above, you can check the JavaScript console to see
   output that your Native Client module prints to stdout and stderr. Keep in
   mind that your module makes a call to PostMessage() every time it flushes
   stdout or stderr.  Your application's performance will degrade considerably
   if your module prints and flushes frequently, or if it makes frequent Pepper
   calls to begin with (e.g., to render).

Logging calls to Pepper interfaces
----------------------------------

You can log all Pepper calls your module makes by passing the following flags
to Chrome on startup::

  --vmodule=ppb*=4 --enable-logging=stderr


The ``vmodule`` flag tells Chrome to log all calls to C Pepper interfaces that
begin with "ppb" (that is, the interfaces that are implemented by the browser
and that your module calls). The ``enable-logging`` flag tells Chrome to log
the calls to stderr.

.. _visual_studio:

Debugging with Visual Studio
----------------------------

If you develop on a Windows platform you can use the Native :doc:`Client Visual
Studio add-in <vs-addin>` to write and debug your code. The add-in defines new
project platforms that let you run your module in two different modes: As a
Pepper plugin and as a Native Client module. When running as a Pepper plugin
you can use the built-in Visual Studio debugger. When running as a Native
Client module Visual Studio will launch an instance of nacl-gdb for you and
link it to the running code.

.. _using_gdb:

Debugging with nacl-gdb
-----------------------

The Native Client SDK includes a command-line debugger that you can use to
debug Native Client modules. The debugger is based on the GNU debugger `gdb
<http://www.gnu.org/software/gdb/>`_, and is located at
``toolchain/<platform>_<architecture>_<library>/bin/<prefix>-nacl-gdb``, where:

* *<platform>* is the platform of your development machine (win, mac, or linux)
* *<architecture>* is your target architecture (x86 or arm)
* *<library>* is the C library you are compiling with (newlib or glibc)
* *<prefix>* depends on the module you are debugging (i686- for x86 32-bit
  modules, x86_64- for x86 64-bit modules, arm- for ARM modules)

For example, to debug an x86 64-bit module built with glibc on Windows, you
would use ``toolchain/win_x86_glibc/bin/x86_64-nacl-gdb``.

.. Note::
  :class: note

  **Prerequisites for using nacl-gdb**:

  * You must use the pepper_23 bundle (or greater) in the SDK.
  * Your version of Chrome must be greater than or equal to the Pepper bundle
    that you are using. For example, if you are using the pepper_23 bundle, you
    must use Chrome 23 or greater. Type about:chrome in the Chrome address bar
    to find out what version of Chrome you have. You may want to install and
    use Chrome Canary on Windows and Mac OS; it's the newest version of Chrome
    that's available, and it runs side-by-side with your current version of
    Chrome.

Before you start using nacl-gdb, make sure you can :doc:`build <building>` your
module and :doc:`run <running>` your application normally. This will verify
that you have created all the required :doc:`application parts
<../coding/application-structure>` (.html, .nmf, and .nexe files, shared
libraries, etc.), that your server can access those resources, and that you've
configured Chrome correctly to run your application.  The instructions below
assume that you are using a :ref:`local server <web_server>` to run your
application; one benefit of doing it this way is that you can check the web
server output to confirm that your application is loading the correct
resources. However, some people prefer to run their application as an unpacked
extension, as described in :doc:`Running Native Client Applications <running>`.

Follow the instructions below to debug your module with nacl-gdb:

#. Compile your module with the ``-g`` flag so that your .nexe retains symbols
   and other debugging information (see the :ref:`recommended compile flags
   <compile_flags>`).
#. Launch a local web server (e.g., the :ref:`web server <web_server>` included
   in the SDK).
#. Launch Chrome with these three required flags: ``--enable-nacl --enable-nacl-debug --no-sandbox``.

   You may also want to use some of the optional flags listed below. A typical
   command looks like this::

     chrome --enable-nacl --enable-nacl-debug --no-sandbox --disable-hang-monitor localhost:5103

   **Required flags:**

   ``--enable-nacl``
     Enables Native Client for all applications, including those that are
     launched outside the Chrome Web Store.

   ``--enable-nacl-debug``
     Turns on the Native Client debug stub, opens TCP port 4014, and pauses
     Chrome to let the debugger connect.

   ``--no-sandbox``
     Turns off the Chrome sandbox (not the Native Client sandbox). This enables
     the stdout and stderr streams, and lets the debugger connect.

   **Optional flags:**

   ``--disable-hang-monitor``
     Prevents Chrome from displaying a warning when a tab is unresponsive.

   ``--user-data-dir=<directory>``
     Specifies the `user data directory
     <http://www.chromium.org/user-experience/user-data-directory>`_ from which
     Chrome should load its state.  You can specify a different user data
     directory so that changes you make to Chrome in your debugging session do
     not affect your personal Chrome data (history, cookies, bookmarks, themes,
     and settings).

   ``<URL>``
     Specifies the URL Chrome should open when it launches. The local server
     that comes with the SDK listens on port 5103 by default, so the URL when
     you're debugging is typically ``localhost:5103`` (assuming that your
     application's page is called index.html and that you run the local server
     in the directory where that page is located).

#. Navigate to your application's page in Chrome. (You don't need to do this if
   you specified a URL when you launched Chrome in the previous step.) Chrome
   will start loading the application, then pause and wait until you start
   nacl-gdb and run the ``continue`` command.

#. Go to the directory with your source code, and run nacl-gdb from there. For
   example::

     cd <NACL_SDK_ROOT>/examples/hello_world_gles
     <NACL_SDK_ROOT>/toolchain/win_x86_newlib/bin/x86_64-nacl-gdb

   The debugger will start and show you a gdb prompt::

     (gdb)

#. Run the following three commands from the gdb command line::

     (gdb) nacl-manifest <path-to-your-.nmf-file>
     (gdb) nacl-irt <path-to-Chrome-NaCl-integrated-runtime>
     (gdb) target remote localhost:4014

   These commands are described below:

   ``nacl-manifest <path>``
     Tells the debugger about your Native Client application by pointing it to
     the application's manifest (.nmf) file. The manifest file lists your
     application's executable (.nexe) files, as well as any libraries that are
     linked with the application dynamically.

   ``nacl-irt <path>``
     Tells the debugger where to find the Native Client Integrated Runtime
     (IRT). The IRT is located in the same directory as the Chrome executable,
     or in a subdirectory named after the Chrome version. For example, if
     you're running Chrome canary on Windows, the path to the IRT typically
     looks something like ``C:/Users/<username>/AppData/Local/Google/Chrome
     SxS/Application/23.0.1247.1/nacl_irt_x86_64.nexe``.

   ``target remote localhost:4014``
     Tells the debugger how to connect to the debug stub in the Native Client
     application loader. This connection occurs through TCP port 4014 (note
     that this port is distinct from the port which the local web server uses
     to listen for incoming requests, typically port 5103).

   A couple of notes on how to specify path names in the nacl-gdb commands
   above:

   * You can use a forward slash to separate directories on Linux, Mac, and
     Windows. If you use a backslash to separate directories on Windows, you
     must escape the backslash by using a double backslash "\\" between
     directories.
   * If any directories in the path have spaces in their name, you must put
     quotation marks around the path.

   As an example, here is a what these nacl-gdb commands might look like on
   Windows::

     nacl-manifest "C:/<NACL_SDK_ROOT>/examples/hello_world_gles/newlib/Debug/hello_world_gles.nmf"
     nacl-irt "C:/Users/<username>/AppData/Local/Google/Chrome SxS/Application/23.0.1247.1/nacl_irt_x86_64.nexe"
     target remote localhost:4014

   To save yourself some typing, you can put put these nacl-gdb commands in a
   script file, and execute the file when you run nacl-gdb, like so::

     <NACL_SDK_ROOT>/toolchain/win_x86_newlib/bin/x86_64-nacl-gdb -x <nacl-script-file>

   If nacl-gdb connects successfully to Chrome, it displays a message such as
   the one below, followed by a gdb prompt::

     0x000000000fc00200 in _start ()
     (gdb)

   If nacl-gdb can't connect to Chrome, it displays a message such as
   "``localhost:4014: A connection attempt failed``" or "``localhost:4014:
   Connection timed out.``" If you see a message like that, make sure that you
   have launched a web server, launched Chrome, and navigated to your
   application's page before starting nacl-gdb.

Once nacl-gdb connects to Chrome, you can run standard gdb commands to execute
your module and inspect its state. Some commonly used commands are listed
below.

``break <location>``
  set a breakpoint at <location>, e.g.::

    break hello_world.cc:79
    break hello_world::HelloWorldInstance::HandleMessage
    break Render

``continue``
  resume normal execution of the program

``next``
  execute the next source line, stepping over functions

``step``
  execute the next source line, stepping into functions

``print <expression>``
  print the value of <expression> (e.g., variables)

``backtrace``
  print a stack backtrace

``info breakpoints``
  print a table of all breakpoints

``delete <breakpoint>``
  delete the specified breakpoint (you can use the breakpoint number displayed
  by the info command)

``help <command>``
  print documentation for the specified gdb <command>

``quit``
  quit gdb

See the `gdb documentation
<http://sourceware.org/gdb/current/onlinedocs/gdb/#toc_Top>`_ for a
comprehensive list of gdb commands. Note that you can abbreviate most commands
to just their first letter (``b`` for break, ``c`` for continue, and so on).

To interrupt execution of your module, press <Ctrl-c>. When you're done
debugging, close the Chrome window and type ``q`` to quit gdb.

Debugging with other tools
==========================

If you cannot use the :ref:`Visual Studio add-in <visual_studio>`, or you want
to use a debugger other than nacl-gdb, you must manually build your module as a
Pepper plugin (sometimes referred to as a `"trusted
<http://www.chromium.org/nativeclient/getting-started/getting-started-background-and-basics#TOC-Trusted-vs-Untrusted>`_"
or "in-process" plugin).  Pepper plugins (.DLL files on Windows; .so files on
Linux; .bundle files on Mac) are loaded directly in either the Chrome renderer
process or a separate plugin process, rather than in Native Client. Building a
module as a trusted Pepper plugin allows you to use standard debuggers and
development tools on your system, but when you're finished developing the
plugin, you need to port it to Native Client (i.e., build the module with one
of the toolchains in the NaCl SDK so that the module runs in Native Client).
For details on this advanced development technique, see `Debugging a Trusted
Plugin
<http://www.chromium.org/nativeclient/how-tos/debugging-documentation/debugging-a-trusted-plugin>`_.
Note that starting with the ``pepper_22`` bundle, the NaCl SDK for Windows
includes pre-built libraries and library source code, making it much easier to
build a module into a .DLL.

Open source profiling tools
---------------------------

For the brave-hearted there are open source tools at `Chromium.org
<http://www.chromium.org/nativeclient>`_ that describe how to do profiling on
`64-bit Windows
<https://sites.google.com/a/chromium.org/dev/nativeclient/how-tos/profiling-nacl-apps-on-64-bit-windows>`_
and `Linux
<http://www.chromium.org/nativeclient/how-tos/limited-profiling-with-oprofile-on-x86-64>`_
machines.


.. |menu-icon| image:: /images/menu-icon.png
.. |puzzle| image:: /images/puzzle.png
