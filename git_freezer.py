#!/usr/local/bin/python
import sys
import re
import optparse

import subcommand
import subprocess2

from git_common import run, stream

FREEZE = 'FREEZE'
SECTIONS = {
  'indexed': 'soft',
  'unindexed': 'mixed'
}
MATCHER = re.compile(r'%s.(%s)' % (FREEZE, '|'.join(SECTIONS)))


def freeze():
  took_action = False

  try:
    run('commit', '-m', FREEZE + '.indexed')
    took_action = True
  except subprocess2.CalledProcessError:
    pass

  try:
    run('add', '-A')
    run('commit', '-m', FREEZE + '.unindexed')
    took_action = True
  except subprocess2.CalledProcessError:
    pass

  if not took_action:
    return 'Nothing to freeze.'


def thaw():
  took_action = False
  for sha in (s.strip() for s in stream('rev-list', 'HEAD').xreadlines()):
    msg = run('show', '--format=%f%b', '-s', 'HEAD')
    match = MATCHER.match(msg)
    if not match:
      if not took_action:
        return 'Nothing to thaw.'
      break

    run('reset', '--' + SECTIONS[match.group(1)], sha)
    took_action = True


def CMDfreeze(parser, args):  # pragma: no cover
  """Freeze a branch's changes."""
  parser.parse_args(args)
  return freeze()


def CMDthaw(parser, args):  # pragma: no cover
  """Returns a frozen branch to the state before it was frozen."""
  parser.parse_args(args)
  return thaw()


def main():  # pragma: no cover
  dispatcher = subcommand.CommandDispatcher(__name__)
  ret = dispatcher.execute(optparse.OptionParser(), sys.argv[1:])
  if ret:
    print ret


if __name__ == '__main__':  # pragma: no cover
  main()