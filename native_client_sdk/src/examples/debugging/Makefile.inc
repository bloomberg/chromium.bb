#
# Setup environment to enable various debugging features, including
# access to the file system, untrusted exception handling, etc...
#

CHROME_ARGS:=--incognito --no-sandbox --enable-nacl

ifneq (,$(wildcard $(CHROME_PATH)))
export NACL_DANGEROUS_ENABLE_FILE_ACCESS=1
export NACL_SECURITY_DISABLE=1
export NACL_UNTRUSTED_EXCEPTION_HANDLING=1
endif


TRACE: CHECK_FOR_CHROME all
	$(CHROME_PATH) $(CHROME_ARGS) "localhost:5103/index.html"
