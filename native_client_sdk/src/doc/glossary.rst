.. _glossary:

########
Glossary
########

This glossary defines terms and names commonly used throughout the Native Client
documentation.

asynchronous programming
  In the asynchronous programming model, function calls are executed and return
  immediately without waiting for a response. Using this model, function calls
  are non-blocking; the web browser continues its main thread of execution
  and gets notified of asynchronous call completion through callbacks or some
  other mechanism.
instance
  A rectangle on a web page that is managed by a Native Client module (the
  rectangle can have ``width=0`` and ``height=0``, which means that nothing is
  drawn on the page).
manifest file
  A file containing metadata or information about accompanying files.
module
  Depending on context, "module" may mean one of two things. First, it may be a
  general short-term for for "Native Client module"---compiled C/C++ code
  produced with a Native Client toolchain (for example PNaCl). See
  :ref:`link_how_nacl_works` for more details.
  Second, it may refer to a concrete implementation of the `pp::Module class
  <https://developers.google.com/native-client/peppercpp/classpp_1_1_module>`_
  for some Native Client module.
Var
  An object in a Native Client module that corresponds to a JavaScript
  variable.
web workers
  `Web workers <http://en.wikipedia.org/wiki/Web_Workers>`_ provide a
  mechanism for running heavy-weight JavaScript code on background threads
  so that the main web page can continue to respond to user interaction.
  Web pages interact with web workers by using ``postMessage()`` to send
  messages. The way a web page interacts with a Native Client module
  is analogous to the way it interacts with web workers.

