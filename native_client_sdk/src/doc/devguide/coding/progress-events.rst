.. _devcycle-progress-events:

:template: standard_nacl_api

###############
Progress Events
###############

.. contents::
  :local:
  :backlinks: none
  :depth: 2

There are five types of events that developers can respond to in Native Client:
progress, message, view change, focus, and input events (each described in the
glossary below). This chapter describes how to monitor progress events (events
that occur during the loading and execution of a Native Client module). This
chapter assumes you are familiar with the material presented in the
:doc:`Technical Overview <../../overview>`.

.. Note::
  :class: note

  The load_progress example illustrates progress event handling. You can find
  this code in the ``/pepper_<version>/examples/tutorial/load_progress/`` 
  directory in the Native Client SDK download.

Module loading and progress events
==================================

The Native Client runtime reports a set of state changes during the module
loading process by means of DOM progress events. This set of events is a direct
port of the proposed W3C `Progress Events
<http://www.w3.org/TR/progress-events/>`_ standard (except for the ``crash``
event which is an extension of the W3C standard). The following table lists the
events types reported by the Native Client runtime:

+-------------+--------------------+-----------+---------------+---------------+
| Event       | Description        | Number of | When event is | How you might |
|             |                    | times     | triggered     | react to      |
|             |                    | triggered |               | event         |
+=============+====================+===========+===============+===============+
|``loadstart``| Native Client has  | once      | This is the   | Display a     |
|             | started to load a  |           | first         | status        |
|             | Native Client      |           | progress      | message, such |
|             | module.            |           | event         | as            |
|             |                    |           | triggered     | "Loading..."  |
|             |                    |           | after the     |               |
|             |                    |           | Native Client |               |
|             |                    |           | module is     |               |
|             |                    |           | instantiated  |               |
|             |                    |           | and           |               |
|             |                    |           | initialized.  |               |
+-------------+--------------------+-----------+---------------+---------------+
|``progress`` | Part of the module | zero or   | After         | Display a     |
|             | has been loaded.   | more      | ``loadstart`` | progress bar. |
|             |                    |           | has been      |               |
|             |                    |           | dispatched.   |               |
+-------------+--------------------+-----------+---------------+---------------+
|``error``    | The Native Client  | zero or   | After the     | Inform user   |
|             | module failed to   | once      | last          | that the      |
|             | start execution    |           | ``progress``  | application   |
|             | (includes any      |           | event has     | failed to     |
|             | error before or    |           | been          | load.         |
|             | during             |           | dispatched,   |               |
|             | initialization of  |           | or after      |               |
|             | the module). The   |           | ``loadstart`` |               |
|             | ``lastError``      |           | if no         |               |
|             | attribute          |           | ``progress``  |               |
|             | (mentioned later)  |           | event was     |               |
|             | provides details   |           | dispatched.   |               |
|             | on the error       |           |               |               |
|             | (initialization    |           |               |               |
|             | failed, sel_ldr    |           |               |               |
|             | did not start,     |           |               |               |
|             | and so on).        |           |               |               |
+-------------+--------------------+-----------+---------------+---------------+
|``abort``    | Loading of the     | zero or   | After the     | It's not      |
|             | Native Client      | once      | last          | likely you    |
|             | module was         |           | ``progress``  | will want to  |
|             | aborted by the     |           | event has     | respond to    |
|             | user.              |           | been          | this event.   |
|             |                    |           | dispatched,   |               |
|             |                    |           | or after      |               |
|             |                    |           | ``loadstart`` |               |
|             |                    |           | if no         |               |
|             |                    |           | ``progress``  |               |
|             |                    |           | event was     |               |
|             |                    |           | dispatched.   |               |
+-------------+--------------------+-----------+---------------+---------------+
|``load``     | The Native Client  | zero or   | After the     | Remove the    |
|             | module was         | once      | last          | progress bar. |
|             | successfully       |           | ``progress``  |               |
|             | loaded, and        |           | event has     |               |
|             | execution was      |           | been          |               |
|             | started. (The      |           | dispatched,   |               |
|             | module was         |           | or after      |               |
|             | initialized        |           | ``loadstart`` |               |
|             | successfully.)     |           | if no         |               |
|             |                    |           | ``progress``  |               |
|             |                    |           | event was     |               |
|             |                    |           | dispatched.   |               |
+-------------+--------------------+-----------+---------------+---------------+
|``loadend``  | Loading of the     | once      | After an      | Indicate      |
|             | Native Client      |           | ``error``,    | loading is    |
|             | module has         |           | ``abort``, or | over          |
|             | stopped. Load      |           | ``load``      | (regardless   |
|             | succeeded          |           | event was     | of failure or |
|             | (``load``),        |           | dispatched.   | not).         |
|             | failed             |           |               |               |
|             | (``error``), or    |           |               |               |
|             | was aborted        |           |               |               |
|             | (``abort``).       |           |               |               |
+-------------+--------------------+-----------+---------------+---------------+
|``crash``    | The Native Client  | zero or   | After a       | Notify user   |
|             | module is not      | once      | ``loadend``.  | that the      |
|             | responding (died   |           |               | module did    |
|             | on an              |           |               | something     |
|             | ``assert()`` or    |           |               | illegal.      |
|             | ``exit()``) after  |           |               |               |
|             | a successful       |           |               |               |
|             | load. This event   |           |               |               |
|             | is unique to       |           |               |               |
|             | Native Client and  |           |               |               |
|             | is not part of     |           |               |               |
|             | the W3C Progress   |           |               |               |
|             | Events standard.   |           |               |               |
|             | The ``exitStatus`` |           |               |               |
|             | attribute provides |           |               |               |
|             | the numeric exit   |           |               |               |
|             | status value.      |           |               |               |
+-------------+--------------------+-----------+---------------+---------------+

The sequence of events for a successful module load is as follows:

=============================== ===============================
Event is dispatched             ... then this task is attempted
=============================== ===============================
``loadstart``                   load the manifest file
``progress`` (first time)       load the module
``progress`` (subsequent times)
``load``                        start executing the module
``loadend``
=============================== ===============================

Errors that occur during loading are logged to the JavaScript console in Google
Chrome (select the menu icon |menu-icon| > Tools > JavaScript console).

.. |menu-icon| image:: /images/menu-icon.png

Handling progress events
========================

You should add event listeners in a ``<script>`` element to listen for these
events before the ``<embed>`` element is parsed. For example, the following code
adds a listener for the ``load`` event to a parent ``<div>`` element that also
contains the Native Client ``<embed>`` element. First, the listener is
attached. Then, when the listener ``<div>`` receives the ``load`` event, the
JavaScript ``moduleDidLoad()`` function is called. The following code is
excerpted from the example in ``getting_started/part1/``:

.. naclcode::

  <!--
  Load the published pexe.
  Note: Since this module does not use any real-estate in the browser, its
  width and height are set to 0.

  Note: The <embed> element is wrapped inside a <div>, which has both a 'load'
  and a 'message' event listener attached.  This wrapping method is used
  instead of attaching the event listeners directly to the <embed> element to
  ensure that the listeners are active before the NaCl module 'load' event
  fires.  This also allows you to use PPB_Messaging.PostMessage() (in C) or
  pp::Instance.PostMessage() (in C++) from within the initialization code in
  your module.
  -->
  <div id="listener">
    <script type="text/javascript">
      var listener = document.getElementById('listener');
      listener.addEventListener('load', moduleDidLoad, true);
      listener.addEventListener('message', handleMessage, true);
    </script>

    <embed id="hello_tutorial"
           width=0 height=0
           src="hello_tutorial.nmf"
           type="application/x-pnacl" />
  </div>

Event listeners can be added to any DOM object. Since listeners set at the
outermost scope capture events for their contained elements, you can set
listeners on outer elements (including the ``<body>`` element) to handle events
from inner elements. For more information, see the W3 specifications for `event
flow capture
<http://www.w3.org/TR/DOM-Level-2-Events/events.html#Events-flow-capture>`_ and
`event listener registration
<http://www.w3.org/TR/DOM-Level-2-Events/events.html#Events-registration>`_.

Displaying load status
======================

One common response to progress events is to display the percentage of the
module that has been loaded. In the load_progress example, when the ``progress``
event is triggered the ``moduleLoadProgress`` function is called. This function
uses the ``lengthComputable``, ``loaded``, and ``total`` attributes (described
in the proposed W3C `Progress Events <http://www.w3.org/TR/progress-events/>`_
standard) of the event to calculate the percentage of the module that has
loaded.

.. naclcode::

  function moduleLoadProgress(event) {
    var loadPercent = 0.0;
    var loadPercentString;
    if (event.lengthComputable && event.total > 0) {
      loadPercent = event.loaded / event.total * 100.0;
      loadPercentString = loadPercent + '%';
      common.logMessage('progress: ' + event.url + ' ' + loadPercentString +
                       ' (' + event.loaded + ' of ' + event.total + ' bytes)');
    } else {
      // The total length is not yet known.
      common.logMessage('progress: Computing...');
    }
  }

The ``lastError`` attribute
===========================

The ``<embed>`` element has a ``lastError`` attribute that is set to an
informative string whenever a load failure (an ``error`` or ``abort`` event)
occurs.

The following code adds an event listener before the ``<embed>`` element to
capture and handle an error in loading the Native Client module. The
``handleError()`` function listens for an ``error`` event. When an error occurs,
this function prints the contents of the ``lastError`` attribute
(``embed_element.lastError``) as an alert.

.. naclcode::

  function domContentLoaded(name, tc, config, width, height) {
    var listener = document.getElementById('listener');
    ...
    listener.addEventListener('error', moduleLoadError, true);
    ...
    common.createNaClModule(name, tc, config, width, height);
  }

  function moduleLoadError() {
    common.logMessage('error: ' + common.naclModule.lastError);
  }

The ``readyState`` attribute
============================

You can use the ``readyState`` attribute to monitor the loading process. This
attribute is particularly useful if you don't care about the details of
individual progress events or when you want to poll for current load state
without registering listeners. The value of ``readyState`` progresses as follows
for a successful load:

===================     ====================
Event                   ``readyState`` value
===================     ====================
(before any events)     ``undefined``
``loadstart``           1
``progress``            3
``load``                4
``loadend``             4
===================     ====================

The following code demonstrates how to monitor the loading process using the
``readyState`` attribute. As before, the script that adds the event listeners
precedes the ``<embed>`` element so that the event listeners are in place before
the progress events are generated.

.. naclcode::

  <html>
  ...
    <body id="body">
      <div id="status_div">
      </div>
      <div id="listener_div">
        <script type="text/javascript">
           var stat = document.getElementById('status_div');
           function handleEvent(e) {
             var embed_element = document.getElementById('my_embed');
             stat.innerHTML +=
             '<br>' + e.type + ': readyState = ' + embed_element.readyState;
           }
           var listener_element = document.getElementById('listener_div');
           listener_element.addEventListener('loadstart', handleEvent, true);
           listener_element.addEventListener('progress', handleEvent, true);
           listener_element.addEventListener('load', handleEvent, true);
           listener_element.addEventListener('loadend', handleEvent, true);
        </script>
        <embed
          name="naclModule"
          id="my_embed"
          width=0 height=0
          src="my_example.nmf"
          type="application/x-pnacl" />
      </div>
    </body>
  </html>

The ``exitStatus`` attribute
============================

This read-only attribute is set if the application calls ``exit(n)``,
``abort()``, or crashes. Since NaCl modules are event handlers, there is no
need to call ``exit(n)`` in normal execution. If the module does exit or
crash, the ``crash`` progress event is issued and the ``exitStatus`` attribute
will contain the numeric value of the exit status:

* In the case of explicit calls to ``exit(n)``, the numeric value will be
  ``n`` (between 0 and 255).
* In the case of crashes and calls to ``abort()``, the numeric value will
  be non-zero, but the exact value will depend on the chosen libc and the
  target architecture, and may change in the future. Applications should not
  rely on the ``exitStatus`` value being stable in these cases, but the value
  may nevertheless be useful for temporary debugging.
