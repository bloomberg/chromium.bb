#!/usr/bin/python

import subprocess
import sys

for line in sys.stdin:
  if line.startswith('SF:'):
    filename = line[3:].strip()
    p = subprocess.Popen(['cygpath', filename], stdout=subprocess.PIPE)
    (p_stdout, _) = p.communicate()
    print 'SF:' + p_stdout.strip()
  else:
    print line.strip()
