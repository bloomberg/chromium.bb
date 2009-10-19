# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import re
import subprocess
import sys
import webbrowser

USAGE = """
WARNING: Please use this tool in an empty directory
(or at least one that you don't mind clobbering.)

REQUIRES: SVN 1.5+
NOTE: NO NEED TO CHECKOUT ANYTHING IN ADVANCE OF USING THIS TOOL."
Valid parameters:

[Merge from trunk to branch]
--merge <revision> --branch <branch_num>
Example: %(app)s --merge 12345 --branch 187

[Revert from trunk]
--revert <revision>
Example: %(app)s --revert 12345

[Revert from branch]
--revert <revision> --branch <branch_num>
Example: %(app)s --revert 12345 --branch 187
"""

export_map_ = None
files_info_ = None
delete_map_ = None
file_pattern_ =  r"[ ]+([MADUC])[ ]+/((?:trunk|branches/\d+)/src(.*)/(.*))"

def deltree(root):
  """Removes a given directory"""
  if (not os.path.exists(root)):
    return

  if sys.platform == 'win32':
    os.system('rmdir /S /Q ' + root.replace('/','\\'))
  else:
    for name in os.listdir(root):
      path = os.path.join(root, name)
      if os.path.isdir(path):
        deltree(path)
      else:
        os.unlink(path)
    os.rmdir(root)

def clobberDir(dir):
  """Removes a given directory"""

  if (os.path.exists(dir)):
    print dir + " directory found, deleting"
    # The following line was removed due to access controls in Windows
    # which make os.unlink(path) calls impossible.
    #TODO(laforge) : Is this correct?
    deltree(dir)

def gclUpload(revision, author):
  command = ("gcl upload " + str(revision) +
             " --send_mail --no_try --no_presubmit --reviewers=" + author)
  os.system(command)

def getSVNInfo(url, revision):
  command = 'svn info ' + url + "@"+str(revision)
  svn_info = subprocess.Popen(command,
                              shell=True,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE).stdout.readlines()
  info = {}
  for line in svn_info:
    match = re.search(r"(.*?):(.*)", line)
    if match:
    	info[match.group(1).strip()]=match.group(2).strip()

  return info

def getAuthor(url, revision):
  info = getSVNInfo(url, revision)
  if (info.has_key("Last Changed Author")):
    return info["Last Changed Author"]
  return None

def isSVNFile(url, revision):
  info = getSVNInfo(url, revision)
  if (info.has_key("Node Kind")):
    if (info["Node Kind"] == "file"):
      return True
  return False

def isSVNDirectory(url, revision):
  info = getSVNInfo(url, revision)
  if (info.has_key("Node Kind")):
    if (info["Node Kind"] == "directory"):
      return True
  return False

def getRevisionLog(url, revision):
  """Takes an svn url and gets the associated revision."""
  command = 'svn log ' + url + " -r"+str(revision)
  svn_log = subprocess.Popen(command, 
                             shell=True, 
                             stdout=subprocess.PIPE, 
                             stderr=subprocess.PIPE).stdout.readlines()
  log = ""
  pos = 0
  for line in svn_log:
    if (pos > 2):
      log += line.replace('-','').replace('\r','')
    else:
      pos = pos + 1
  return log

def checkoutRevision(url, revision, branch_url, revert=False):
  files_info = getFileInfo(url, revision)
  paths = getBestMergePaths2(files_info, revision)
  export_map = getBestExportPathsMap2(files_info, revision)

  command = 'svn checkout -N ' + branch_url
  print command
  os.system(command)

  match = re.search(r"svn://.*/(.*)", branch_url)

  if match:
    os.chdir(match.group(1))

  # This line is extremely important due to the way svn behaves in the
  # set-depths action.  If parents aren't handled before children, the child
  # directories get clobbered and the merge step fails.
  paths.sort()

  # Checkout the directories that already exist 
  for path in paths:
     if (export_map.has_key(path) and not revert):
       print "Exclude new directory " + path
       continue
     subpaths = path.split('/')
     subpaths.pop(0)
     base = ''
     for subpath in subpaths:
       base += '/' + subpath
       # This logic ensures that you don't empty out any directories
       if not os.path.exists("." + base):
         command = ('svn update --depth empty ' + "." + base)
         print command
         os.system(command)

  if (revert):
    files = getAllFilesInRevision(files_info)
  else:
    files = getExistingFilesInRevision(files_info)
  
  for file in files:
   # Prevent the tool from clobbering the src directory 
    if (file == ""):
      continue
    command = ('svn up ".' + file + '"')
    print command
    os.system(command)

def mergeRevision(url, revision):
  paths = getBestMergePaths(url, revision)
  export_map = getBestExportPathsMap(url, revision)

  for path in paths:
    if export_map.has_key(path):
      continue
    command = ('svn merge -N -r ' + str(revision-1) + ":" + str(revision) + " ")
    command = command + url + path + "@" + str(revision) + " ." + path

    print command
    os.system(command)

def exportRevision(url, revision):
  paths = getBestExportPathsMap(url, revision).keys()
  paths.sort()

  for path in paths:
    command = ('svn export -N ' + url + path + "@" + str(revision) + " ."  +
               path)
    print command
    os.system(command)

    command = 'svn add .' + path
    print command 
    os.system(command)

def deleteRevision(url, revision):
  paths = getBestDeletePathsMap(url, revision).keys()
  paths.sort()
  paths.reverse()

  for path in paths:
    command = "svn delete ." + path
    print command
    os.system(command)

def revertExportRevision(url, revision):
  paths = getBestExportPathsMap(url, revision).keys()
  paths.sort()
  paths.reverse()

  for path in paths:
    command = "svn delete ." + path
    print command
    os.system(command)

def revertRevision(url, revision):
  paths = getBestMergePaths(url, revision)
  for path in paths:
    command = ('svn merge -N -r ' + str(revision) + ":" + str(revision-1) + 
                " " + url + path + " ." + path)
    print command
    os.system(command)

def getFileInfo(url, revision):
  global files_info_, file_pattern_

  if (files_info_ != None):
    return files_info_

  command = 'svn log ' + url + " -r " + str(revision) + " -v"
  svn_log = subprocess.Popen(command,
                             shell=True,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE).stdout.readlines()

  info = []
  for line in svn_log:
    # A workaround to dump the (from .*) stuff, regex not so friendly in the 2nd
    # pass...
    match = re.search(r"(.*) \(from.*\)", line)
    if match:
      line = match.group(1)
    match = re.search(file_pattern_, line)
    if match:
      info.append([match.group(1).strip(), match.group(2).strip(),
                   match.group(3).strip(),match.group(4).strip()])

  files_info_ = info
  return info

def getBestMergePaths(url, revision):
  """Takes an svn url and gets the associated revision."""
  return getBestMergePaths2(getFileInfo(url, revision), revision)

def getBestMergePaths2(files_info, revision):
  """Takes an svn url and gets the associated revision."""

  map = dict()
  for file_info in files_info:
    map[file_info[2]] = file_info[2]

  return map.keys()

def getBestExportPathsMap(url, revision):
  return getBestExportPathsMap2(getFileInfo(url, revision), revision)

def getBestExportPathsMap2(files_info, revision):
  """Takes an svn url and gets the associated revision."""
  global export_map_

  if export_map_:
    return export_map_

  map = dict()
  for file_info in files_info:
    if (file_info[0] == "A"):
      if(isSVNDirectory("svn://chrome-svn/chrome/" + file_info[1], revision)):
        map[file_info[2] + "/" + file_info[3]] = ""

  export_map_ = map
  return map

def getBestDeletePathsMap(url, revision):
  return getBestDeletePathsMap2(getFileInfo(url, revision), revision)

def getBestDeletePathsMap2(files_info, revision):
  """Takes an svn url and gets the associated revision."""
  global delete_map_

  if delete_map_:
    return delete_map_

  map = dict()
  for file_info in files_info:
    if (file_info[0] == "D"):
      if(isSVNDirectory("svn://chrome-svn/chrome/" + file_info[1], revision)):
        map[file_info[2] + "/" + file_info[3]] = ""

  delete_map_ = map
  return map

def getExistingFilesInRevision(files_info):
  """Checks for existing files in the revision.
 
  Anything that's A will require special treatment (either a merge or an
  export + add)
  """
  map = []
  for file_info in files_info:
    if file_info[0] != "A": 
      map.append(file_info[2] + "/" + file_info[3])

  return map

def getAllFilesInRevision(files_info):
  """Checks for existing files in the revision.

  Anything that's A will require special treatment (either a merge or an
  export + add)
  """
  map = []
  for file_info in files_info:
    map.append(file_info[2] + "/" + file_info[3])
  return map

def prompt(question):
  answer = None

  while not answer:
    print question + " [y|n]:"
    answer = sys.stdin.readline()
    if answer.lower().startswith('n'):
      return False
    elif answer.lower().startswith('y'):
      return True
    else:
      answer = None 

def text_prompt(question, default):
  print question + " [" + default + "]:"
  answer = sys.stdin.readline()
  if answer.strip() == "":
    return default
  return answer

def main(options, args):
  BASE_URL = "svn://chrome-svn/chrome"
  TRUNK_URL = BASE_URL + "/trunk/src"
  BRANCH_URL = BASE_URL + "/branches/$branch/src"
  DEFAULT_WORKING = "working"
  SKIP_CHECK_WORKING = True
  PROMPT_FOR_AUTHOR = False

  # Override the default properties if there is a drover.properties file.
  global file_pattern_
  if os.path.exists("drover.properties"):
    file = open("drover.properties")
    exec(file)
    file.close()
    if FILE_PATTERN:
      file_pattern_ = FILE_PATTERN   

  revision = options.revert or options.merge

  if options.revert and options.branch:
    url = BRANCH_URL.replace("$branch", options.branch) 
  else:
    url = TRUNK_URL

  working = DEFAULT_WORKING

  command = 'svn log ' + url + " -r "+str(revision) + " -v"
  os.system(command)

  if not (options.revertbot or prompt("Is this the correct revision?")):
    sys.exit(0)

  if (os.path.exists(working)):
    if not (options.revertbot or SKIP_CHECK_WORKING or
        prompt("Working directory: '%s' already exists, clobber?" % working)):
      sys.exit(0)
    deltree(working)

  os.makedirs(working)
  os.chdir(working)

  if options.merge:
    action = "Merge"
    branch_url = BRANCH_URL.replace("$branch", options.branch)
    # Checkout everything but stuff that got added into a new dir
    checkoutRevision(url, revision, branch_url)
    # Merge everything that changed
    mergeRevision(url, revision)
    # "Export" files that were added from the source and add them to branch
    exportRevision(url, revision)
    # Delete directories that were deleted (file deletes are handled in the
    # merge).
    deleteRevision(url, revision)
  elif options.revert:
    action = "Revert"
    if options.branch:
      url = BRANCH_URL.replace("$branch", options.branch)
    checkoutRevision(url, revision, url, True)
    revertRevision(url, revision)
    revertExportRevision(url, revision)

  # Check the base url so we actually find the author who made the change
  author = getAuthor(TRUNK_URL, revision)

  filename = str(revision)+".txt"
  out = open(filename,"w")
  out.write(action +" " + str(revision) + " - ")
  out.write(getRevisionLog(url, revision))
  if (author):
    out.write("TBR=" + author)
  out.close()

  change_cmd = 'gcl change ' + str(revision) + " " + filename
  if options.revertbot:
    change_cmd += ' --silent'
  os.system(change_cmd)
  os.unlink(filename)
  print author
  print revision
  print ("gcl upload " + str(revision) +
         " --send_mail --no_try --no_presubmit --reviewers=" + author)

  if options.revertbot or prompt("Would you like to upload?"):
    if PROMPT_FOR_AUTHOR:
      author = text_prompt("Enter new author or press enter to accept default",
                           author)
    if options.revertbot and options.revertbot_reviewers:
      author += ","
      author += options.revertbot_reviewers
    gclUpload(revision, author)
  else:
    print "Deleting the changelist."
    print "gcl delete " + str(revision)
    os.system("gcl delete " + str(revision))
    sys.exit(0)

  # We commit if the reverbot is set to commit automatically, or if this is
  # not the revertbot and the user agrees.
  if options.revertbot_commit or (not options.revertbot and
                                  prompt("Would you like to commit?")):
    print "gcl commit " + str(revision) + " --no_presubmit --force"
    os.system("gcl commit " + str(revision) + " --no_presubmit --force")
  else:
    sys.exit(0)

if __name__ == "__main__":
  option_parser = optparse.OptionParser(usage=USAGE % {"app": sys.argv[0]})
  option_parser.add_option('-m', '--merge', type="int",
                           help='Revision to merge from trunk to branch')
  option_parser.add_option('-b', '--branch',
                           help='Branch to revert or merge from')
  option_parser.add_option('-r', '--revert', type="int",
                           help='Revision to revert')
  option_parser.add_option('', '--revertbot', action='store_true',
                           default=False)
  option_parser.add_option('', '--revertbot-commit', action='store_true',
                           default=False)
  option_parser.add_option('', '--revertbot-reviewers')
  options, args = option_parser.parse_args()

  if not options.merge and not options.revert:
    option_parser.error("You need at least --merge or --revert")
    sys.exit(1)

  if options.merge and not options.branch:
    option_parser.error("--merge requires a --branch")
    sys.exit(1)

  sys.exit(main(options, args))