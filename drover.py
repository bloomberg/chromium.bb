import subprocess
import sys
import re
import os
import webbrowser

def deltree(root):
  """
  Removes a given directory
  """
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
  """
  Removes a given directory
  """
  
  if (os.path.exists(dir)):
      print dir + " directory found, deleting"
      #The following line was removed due to access controls in Windows
      #which make os.unlink(path) calls impossible.
      #deltree(dir)
      os.system('rmdir /S /Q ' + dir.replace('/','\\'))

def gclUpload(revision, author):
  command = "gcl upload " + str(revision) + " --send_mail --no_try --no_presubmit --reviewers=" + author
  os.system(command)
#  subprocess.Popen(command, 
#                   shell=True, 
#                   stdout=None, 
#                   stderr=subprocess.PIPE)
#                              stderr=subprocess.PIPE).stdout.readlines()
#  for line in svn_info:
#    match = re.search(r"Issue created. URL: (http://.+)", line)
#    if match:
#    	return match.group(1)
  
  return None  

def getAuthor(url, revision):
  command = 'svn info ' + url + "@"+str(revision)
  svn_info = subprocess.Popen(command, 
                              shell=True, 
                              stdout=subprocess.PIPE, 
                              stderr=subprocess.PIPE).stdout.readlines()
  for line in svn_info:
    match = re.search(r"Last Changed Author: (.+)", line)
    if match:
    	return match.group(1)
  
  return None


def getRevisionLog(url, revision):
  """
  Takes an svn url and gets the associated revision.
  """
  command = 'svn log ' + url + " -r"+str(revision)
  svn_info = subprocess.Popen(command, 
                              shell=True, 
                              stdout=subprocess.PIPE, 
                              stderr=subprocess.PIPE).stdout.readlines()
  rtn= ""
  pos = 0
  for line in svn_info:
    if (pos > 2):
      rtn += line.replace('-','').replace('\r','')
    else:
      pos = pos + 1

  return rtn

def checkoutRevision(url, revision, branch_url):
  paths = getBestMergePaths(url, revision)
  
  deltree('./src')
  
  if not os.path.exists('./src'):
    command = 'svn checkout -N ' + branch_url
    print command
    os.system(command)

  #This line is extremely important due to the way svn behaves in the set-depths
  #action.  If parents aren't handled before children, the child directories get
  #clobbered and the merge step fails.
  paths.sort()
  
  for path in paths:
     subpaths = path.split('/')
     subpaths.pop(0)
     base = './src'
     for subpath in subpaths:
       base += '/' + subpath     
       if not os.path.exists(base):
         command = ('svn update --depth empty ' + base) 
         print command
         os.system(command)
       else:
         print "Found " + base
  
  files = getFilesInRevision(url, revision)
  
  for file in files:
   #Prevent the tool from clobbering the src directory  
    if (file == ""):
      continue
    command = ('svn up ./src' + file)
    print command
    os.system(command)
  
#def mergeRevision(url, revision):
#  command = 'svn merge -r ' + str(revision-1) + ":" + str(revision) + " " + url
#  print command
#  os.system(command)

def mergeRevision(url, revision, ignoreAncestry=False):
  paths = getBestMergePaths(url, revision)
  for path in paths:
    command = ('svn merge -N -r ' + str(revision-1) + ":" + str(revision) + " ")
    if (ignoreAncestry):
      command = command + " --ignore-ancestry " 
    command = command + url + path + " ./src" + path
     
    print command
    os.system(command)

def revertRevision(url, revision):
  paths = getBestMergePaths(url, revision)
  for path in paths:
    command = ('svn merge -N -r ' + str(revision) + ":" + str(revision-1) + " " + 
                url + path + " ./src" + path) 
    print command
    os.system(command)

def getBestMergePaths(url, revision):
  """
  Takes an svn url and gets the associated revision.
  """
  command = 'svn log ' + url + " -r "+str(revision) + " -v"
  svn_info = subprocess.Popen(command, 
                              shell=True, 
                              stdout=subprocess.PIPE, 
                              stderr=subprocess.PIPE).stdout.readlines()
  map = dict()
  for line in svn_info:
    #match = re.search(r"[\n\r ]+[MADUC][\n\r ]+/.*/src(.*)/.+", line)
    #match = re.search(r"[\n\r ]+[MADUC][\n\r ]+/(?:trunk|branches/\d+)/src(.*)/.+", line)
    match = re.search(r"[\n\r ]+[MADUC][\n\r ]+/(?:trunk|branches/\d+)/src([^ ]*)/[^ ]+", line)
    
    if match:
      map[match.group(1)] = match.group(1)

  return map.keys()

def getFilesInRevision(url, revision):
  """
  Takes an svn url and gets the associated revision.
  """
  command = 'svn log ' + url + " -r "+str(revision) + " -v"
  svn_info = subprocess.Popen(command, 
                              shell=True, 
                              stdout=subprocess.PIPE, 
                              stderr=subprocess.PIPE).stdout.readlines()
  map = dict()
  for line in svn_info:
    match = re.search(r"[\n\r ]+[MADUC][\n\r ]+/(?:trunk|branches/\d+)/src([^ ]*)/([^ ]+)", line)
    
    if match:
      map[match.group(1) + "/" + match.group(2)] = match.group(1) + "/" + match.group(2)

  return map.keys()

def getBestMergePath(url, revision):
  """
  Takes an svn url and gets the associated revision.
  """
  command = 'svn log ' + url + " -r "+str(revision) + " -v"
  svn_info = subprocess.Popen(command, 
                              shell=True, 
                              stdout=subprocess.PIPE, 
                              stderr=subprocess.PIPE).stdout.readlines()
  best_path = None
  
  for line in svn_info:
    match = re.search(r"[\n\r ]+[MADUC][\n\r ]+/.*/src(.*)/.+", line)
    if match:
      if (best_path == None):
        best_path = match.group(1)
      else:
        best_path = leastPath(match.group(1),best_path)
#      print best_path

  return best_path

def leastPath(a, b):
  if (not a) or (a == ""):
    return ""
  if (b == ""):
    return ""
  if (not b):
    return a 
    
  a_list = a.lstrip("/").split("/")
  b_list = b.lstrip("/").split("/")
  
  last_match = ""
  while((len(a_list) != 0) and (len(b_list) != 0)):
    a_value = a_list.pop(0)
    b_value = b_list.pop(0)
    if (a_value == b_value):
      last_match = last_match + "/" + a_value
    else:
      break
  
  return last_match

def prompt(question):
  p = None
  
  while not p:
    print question + " [y|n]:"
    p = sys.stdin.readline()
    if p.lower().startswith('n'):
      return False
    elif p.lower().startswith('y'):
      return True
    else:
      p = None 

def main(argv=None):
  BASE_URL = "svn://chrome-svn/chrome"
  TRUNK_URL = BASE_URL + "/trunk/src"
  BRANCH_URL = None

  if (len(sys.argv) == 1):
    print "WARNING: Please use this tool in an empty directory (or at least one"
    print "that you don't mind clobbering."
    print "REQUIRES: SVN 1.5+"
    print "NOTE: NO NEED TO CHECKOUT ANYTHING IN ADVANCE OF USING THIS TOOL."
    print "\nValid parameters:"
    print "\n[Merge from trunk to branch]"
    print "<revision> --merge <branch_num>"
    print "Example " + sys.argv[0] + " 12345 --merge 187"
    print "\n[Merge from trunk to branch, ignoring revision history]"
    print "<revision> --mplus <branch_num>"
    print "Example " + sys.argv[0] + " 12345 --mplus 187"
    print "\n[Revert from trunk]"
    print " <revision> --revert"
    print "Example " + sys.argv[0] + " 12345 --revert"
    print "\n[Revert from branch]"
    print " <revision> --revert <branch_num>"
    print "Example " + sys.argv[0] + " 12345 --revert 187"
    sys.exit(0)

  revision = int(sys.argv[1])
  if ((len(sys.argv) >= 4) and (sys.argv[2] in ['--revert','-r'])):
    BRANCH_URL = BASE_URL + "/branches/" + sys.argv[3] + "/src"
    url = BRANCH_URL 
  else:
    url = TRUNK_URL
  action = "Merge"
  
  command = 'svn log ' + url + " -r "+str(revision) + " -v"
  os.system(command)
  
  if not prompt("Is this the correct revision?"):
    sys.exit(0)
  
  if (len(sys.argv) > 1):
     if sys.argv[2] in ['--merge','-m']:
        if (len(sys.argv) != 4):
          print "Please specify the branch # you want (i.e. 182) after --merge"
          sys.exit(0)
           
        branch_url = "svn://chrome-svn/chrome/branches/" + sys.argv[3] + "/src"
        checkoutRevision(url, revision, branch_url)
        mergeRevision(url, revision)
     elif sys.argv[2] in ['--mplus','-p']:
        if (len(sys.argv) != 4):
          print "Please specify the branch # you want (i.e. 182) after --merge"
          sys.exit(0)
        branch_url = "svn://chrome-svn/chrome/branches/" + sys.argv[3] + "/src"
        checkoutRevision(url, revision, branch_url)
        mergeRevision(url, revision, True)        
     elif sys.argv[2] in ['--revert','-r']:
       if (len(sys.argv) == 4):
         url = "svn://chrome-svn/chrome/branches/" + sys.argv[3] + "/src"
       checkoutRevision(url, revision, url)
       revertRevision(url, revision)
       action = "Revert"
     else:
       print "Unknown parameter " + sys.argv[2]
       sys.exit(0)

  os.chdir('./src')
  
  #Check the base url so we actually find the author who made the change
  author = getAuthor(BASE_URL, revision)
  
  filename = str(revision)+".txt"
  out = open(filename,"w")
  out.write(action +" " + str(revision) + " - ")
  out.write(getRevisionLog(url, revision))
  if (author):
    out.write("TBR=" + author)
  out.close()
  
  os.system('gcl change ' + str(revision) + " " + filename)
  os.unlink(filename)
  print author
  print revision
  print "gcl upload " + str(revision) + " --send_mail --no_try --no_presubmit --reviewers=" + author
  print "gcl commit " + str(revision) + " --no_presubmit --force"
  print "gcl delete " + str(revision)

  if prompt("Would you like to upload?"):
    gclUpload(revision, author)
  else:
    print "Deleting the changelist."
    os.system("gcl delete " + str(revision))
    sys.exit(0)

  if prompt("Would you like to commit?"):
    os.system("gcl commit " + str(revision) + " --no_presubmit --force")
  else:
    sys.exit(0)
  
if __name__ == "__main__":
  sys.exit(main())