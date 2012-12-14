# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Operation, including output and progress display

This module implements the concept of an operation, which has regular progress
updates, verbose text display and perhaps some errors.
"""

import contextlib
import os
import re
import sys
from chromite.lib.terminal import Color


#TODO(sjg): When !isatty(), keep stdout and stderr separate so they can be
#redirected separately
#TODO(sjg): Add proper docs to this fileno
#TODO(sjg): Handle stdin wait in quite mode, rather than silently stalling

class Operation:
  """Class which controls stdio and progress of an operation in progress.

  This class is created to handle stdio for a running subprocess. It filters
  it looking for errors and progress information. Optionally it can output the
  stderr and stdout to the terminal, but it is normally supressed.

  Progress information is garnered from the subprocess output based on
  knowledge of the legacy scripts, but at some point will move over to using
  real progress information reported through new python methods which will
  replace the scripts.

  Each operation has a name, and this class handles displaying this name
  as it reports progress.


  Operation Objects
  =================

  verbose: True / False
    In verbose mode all output from subprocesses is displayed, otherwise
    this output is normally supressed, unless we think it indicates an error.

  progress: True / False
    The output from subprocesses can be analysed in a very basic manner to
    try to present progress information to the user.

  explicit_verbose: True / False
    False if we are not just using default verbosity. In that case we allow
    verbosity to be enabled on request, since the user has not explicitly
    disabled it. This is used by commands that the user issues with the
    expectation that output would ordinarily be visible.
  """
  # Force color on/off, or use color only if stdout is a terminal.
  COLOR_OFF, COLOR_ON, COLOR_IF_TERMINAL = range(3)

  def __init__(self, name, color=COLOR_IF_TERMINAL):
    """Create a new operation.

    Args:
      name: Operation name in a form to be displayed for the user.
      color: Determines policy for sending color to stdout:
        COLOR_OFF: never send color.
        COLOR_ON: always send color.
        COLOR_IF_TERMINAL: send color if output apperas to be a terminal.
    """
    self._name = name   # Operation name.
    self.verbose = False   # True to echo subprocess output.
    self.progress = True   # True to report progress of the operation
    self._column = 0    # Current output column (always 0 unless verbose).
    self._update_len = 0    # Length of last progress update message.
    self._line = ''   # text of current line, so far
    self.explicit_verbose = False

    # By default, we display ANSI colors unless output is redirected.
    want_color = (color == self.COLOR_ON or
      (color == self.COLOR_IF_TERMINAL and os.isatty(sys.stdout.fileno())))
    self._color = Color(want_color)

    # -1 = no newline pending
    #  n = newline pending, and line length of last line was n
    self._pending_nl = -1

    # the type of the last stream to emit data on the current lines
    # can be sys.stdout, sys.stderr (both from the subprocess), or None
    # for our own mesages
    self._cur_stream = None

    self._error_count = 0   # number of error lines we have reported

  def __del__(self):
    """Object is about to be destroyed, so finish out output cleanly."""
    self.FinishOutput()

  def FinishOutput(self):
    """Finish off any pending output.

    This finishes any output line currently in progress and resets the color
    back to normal.
    """
    self._FinishLine(self.verbose, final=True)
    if self._column and self.verbose:
      print self._color.Stop()
      self._column = 0

  def WereErrorsDetected(self):
    """Returns whether any errors have been detected.

    Returns:
      True if any errors have been detected in subprocess output so far.
      False otherwise
    """
    return self._error_count > 0

  def SetName(self, name):
    """Set the name of the operation as displayed to the user.

    Args:
      name: Operation name.
    """
    self._name = name

  def _FilterOutputForErrors(self, line, print_error):
    """Filter a line of output to look for and display errors.

    This uses a few regular expression searches to spot common error reports
    from subprocesses. A count of these is kept so we know how many occurred.
    Optionally they are displayed in red on the terminal.

    Args:
      line: the output line to filter, as a string.
      print_error: True to print the error, False to just record it.
    """
    bad_things = ['Cannot GET', 'ERROR', '!!!', 'FAILED']
    for bad_thing in bad_things:
      if re.search(bad_thing, line, flags=re.IGNORECASE):
        self._error_count += 1
        if print_error:
          print self._color.Color(self._color.RED, line)
          break

  def _FilterOutputForProgress(self, line):
    """Filter a line of output to look for and dispay progress information.

    This uses a simple regular expression search to spot progress information
    coming from subprocesses. This is sent to the _Progress() method.

    Args:
      line: the output line to filter, as a string.
    """
    match = re.match('Pending (\d+).*Total (\d+)', line)
    if match:
      pending = int(match.group(1))
      total = int(match.group(2))
      self._Progress(total - pending, total)

  def _Progress(self, upto, total):
    """Record and optionally display progress information.

    Args:
      upto: which step we are up to in the operation (integer, from 0).
      total: total number of steps in operation,
    """
    if total > 0:
      update_str = '%s...%d%% (%d of %d)' % (self._name,
          upto * 100 // total, upto, total)
      if self.progress:
        # Finish the current line, print progress, and remember its length.
        self._FinishLine(self.verbose)

        # Sometimes the progress string shrinks and in this case we need to
        # blank out the characters at the end of the line that will not be
        # overwritten by the new line
        pad = max(self._update_len - len(update_str), 0)
        sys.stdout.write(update_str + (' ' * pad) + '\r')
        self._update_len = len(update_str)

  def _FinishLine(self, display, final=False):
    """Finish off the current line and prepare to start a new one.

    If a new line is pending from the previous line, then this will be output,
    along with a color reset if needed.

    We also handle removing progress messages from the output. This is done
    using a carriage return character, following by spaces.

    Args:
      display: True to display output, False to suppress it
      final: True if this is the final output before we exit, in which case
          we must clean up any remaining progress message by overwriting
          it with spaces, then carriage return
    """
    if display:
      if self._pending_nl != -1:
        # If out last output line was shorter than the progress info
        # add spaces.
        if self._pending_nl < self._update_len:
          print ' ' * (self._update_len - self._pending_nl),

        # Output the newline, and reset our counter.
        sys.stdout.write(self._color.Stop())
        print

    # If this is the last thing that this operation will print, we need to
    # close things off. So if there is some text on the current line but not
    # enough to overwrite all the progress information we have sent, add some
    # more spaces.
    if final and self._update_len:
      print ' ' * self._update_len, '\r',

    self._pending_nl = -1

  def _CheckStreamAndColor(self, stream, display):
    """Check that we're writing to the same stream as last call.  No?  New line.

    If starting a new line, set the color correctly:
      stdout  Magenta
      stderr  Red
      other   White / no colors

    Args:
      stream: The stream we're going to write to.
      display: True to display it on terms, False to suppress it.
    """
    if self._column > 0 and stream != self._cur_stream:
      self._FinishLine(display)
      if display:
        print self._color.Stop()

      self._column = 0
      self._line = ''

    # Use colors for child output.
    if self._column == 0:
      self._FinishLine(display)
      if display:
        color = None
        if stream == sys.stdout:
          color = self._color.MAGENTA
        elif stream == sys.stderr:
          color = self._color.RED
        if color:
          sys.stdout.write(self._color.Start(color))

      self._cur_stream = stream

  def _Out(self, stream, text, display, newline=False, do_output_filter=True):
    """Output some text received from a child, or generated internally.

    This method is the guts of the Operation class since it understands how to
    convert a series of output requests on different streams into something
    coherent for the user.

    If the stream has changed, then a new line is started even if we were
    still halfway through the previous line. This prevents stdout and stderr
    becoming mixed up quite so badly.

    We use color to indicate lines which are stdout and stderr. If the output
    received from the child has color codes in it already, we pass these
    through, so our colors can be overridden. If output is redirected then we
    do not add color by default. Note that nothing stops the child from adding
    it, but since we present ourselves as a terminal to the child, one might
    hope that the child will not generate color.

    If display is False, then we will not actually send this text to the
    terminal. This is uses when verbose is required to be False.

    Args:
      stream: stream on which the text was received:
        sys.stdout    - received on stdout
        sys.stderr    - received on stderr
        None          - generated by us / internally
      text: text to output
      display: True to display it on terms, False to suppress it
      newline: True to start a new line after this text, False to put the next
        lot of output immediately after this.
      do_output_filter: True to look through output for errors and progress.
    """
    self._CheckStreamAndColor(stream, display)

    # Output what we have, and remember what column we are up to.
    if display:
      sys.stdout.write(text)
      self._column += len(text)
      # If a newline is required, remember to output it later.
      if newline:
        self._pending_nl = self._column
        self._column = 0

    self._line += text

    # If we now have a whole line, check it for errors and progress.
    if newline:
      if do_output_filter:
        self._FilterOutputForErrors(self._line, print_error=not display)
        self._FilterOutputForProgress(self._line)
      self._line = ''

  def Output(self, stream, data):
    """Handle the output of a block of text from the subprocess.

    All subprocess output should be sent through this method. It is split into
    lines which are processed separately using the _Out() method.

    Args:
      stream: Which file the output come in on:
        sys.stdout: stdout
        sys.stderr: stderr
        None: Our own internal output
      data: Output data as a big string, potentially containing many lines of
        text. Each line should end with \r\n. There is no requirement to send
        whole lines - this method happily handles fragments and tries to
        present then to the user as early as possible

    #TODO(sjg): Just use a list as the input parameter to avoid the split.
    """
    # We cannot use splitlines() here as we need this exact behavior
    lines = data.split('\r\n')

    # Output each full line, with a \n after it.
    for line in lines[:-1]:
      self._Out(stream, line, display=self.verbose, newline=True)

    # If we have a partial line at the end, output what we have.
    # We will continue it later.
    if lines[-1]:
      self._Out(stream, lines[-1], display=self.verbose)

    # Flush so that the terminal will receive partial line output (now!)
    sys.stdout.flush()

  def Outline(self, line):
    """Output a line of text to the display.

    This outputs text generated internally, such as a warning message or error
    summary. It ensures that our message plays nicely with child output if
    any.

    Args:
      line: text to output (without \n on the end)
    """
    self._Out(None, line, display=True, newline=True)
    self._FinishLine(display=True)

  def Info(self, line):
    """Output a line of information text to the display in verbose mode.

    Args:
      line: text to output (without \n on the end)
    """
    self._Out(None, self._color.Color(self._color.BLUE, line),
        display=self.verbose, newline=True, do_output_filter=False)
    self._FinishLine(display=True)

  def Notice(self, line):
    """Output a line of notification text to the display.

    Args:
      line: text to output (without \n on the end)
    """
    self._Out(None, self._color.Color(self._color.GREEN, line),
        display=True, newline=True, do_output_filter=False)
    self._FinishLine(display=True)

  def Warning(self, line):
    """Output a line of warning text to the display.

    Args:
      line: text to output (without \n on the end)
    """
    self._Out(None, self._color.Color(self._color.YELLOW, line),
        display=True, newline=True, do_output_filter=False)
    self._FinishLine(display=True)

  def Error(self, line):
    """Output a line of error text to the display.

    Args:
      line: text to output (without \n on the end)
    """
    self._Out(None, self._color.Color(self._color.RED, line),
        display=True, newline=True, do_output_filter=False)
    self._FinishLine(display=True)

  def Die(self, line):
    """Output a line of error text to the display and die.

    Args:
      line: text to output (without \n on the end)
    """
    self.Error(line)
    sys.exit(1)

  @contextlib.contextmanager
  def RequestVerbose(self, request):
    """Perform something in verbose mode if the user hasn't disallowed it

    This is intended to be used with something like:

      with oper.RequestVerbose(True):
        ... do some things that generate output

    Args:
      request: True to request verbose mode if available, False to do nothing.

    """
    old_verbose = self.verbose
    if request and not self.explicit_verbose:
      self.verbose = True
    try:
      yield
    finally:
      self.verbose = old_verbose
