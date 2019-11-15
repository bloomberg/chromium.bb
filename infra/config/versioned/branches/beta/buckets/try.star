load('//versioned/vars/try.star', 'vars')
vars.bucket.set('try-beta')
vars.cq_group.set('cq-beta')
vars.experiment_percentage.set(100)

load('//versioned/milestones.star', milestone='beta')
exec('//versioned/milestones/%s/buckets/try.star' % milestone)
