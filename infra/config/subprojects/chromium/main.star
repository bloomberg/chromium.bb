load('//project.star', 'master_only_exec')

exec('./ci.star')
exec('./try.star')

master_only_exec('./master-only/main.star')
